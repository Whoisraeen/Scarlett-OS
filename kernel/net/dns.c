/**
 * @file dns.c
 * @brief DNS resolver implementation
 */

#include "../include/net/dns.h"
#include "../include/net/udp.h"
#include "../include/net/ip.h"
#include "../include/net/network.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"
#include "../include/sync/spinlock.h"

// DNS state
static struct {
    uint32_t nameserver_ip;
    uint16_t next_id;
    spinlock_t lock;
    bool initialized;
} dns_state = {0};

// Default nameserver (Google DNS)
#define DNS_DEFAULT_NAMESERVER 0x08080808  // 8.8.8.8

/**
 * Encode domain name for DNS
 */
static size_t dns_encode_name(const char* name, uint8_t* buffer, size_t buffer_size) {
    if (!name || !buffer || buffer_size < 1) {
        return 0;
    }
    
    size_t pos = 0;
    const char* start = name;
    
    while (*name) {
        if (*name == '.') {
            size_t len = name - start;
            if (len > 63 || pos + len + 1 > buffer_size) {
                return 0;
            }
            buffer[pos++] = (uint8_t)len;
            memcpy(buffer + pos, start, len);
            pos += len;
            start = name + 1;
        }
        name++;
    }
    
    // Final label
    size_t len = name - start;
    if (len > 0) {
        if (len > 63 || pos + len + 1 > buffer_size) {
            return 0;
        }
        buffer[pos++] = (uint8_t)len;
        memcpy(buffer + pos, start, len);
        pos += len;
    }
    
    // Null terminator
    if (pos + 1 > buffer_size) {
        return 0;
    }
    buffer[pos++] = 0;
    
    return pos;
}

/**
 * Decode domain name from DNS
 */
static size_t dns_decode_name(const uint8_t* buffer, size_t buffer_size, size_t offset, char* name, size_t name_size) {
    if (!buffer || !name || offset >= buffer_size || name_size < 1) {
        return 0;
    }
    
    size_t pos = 0;
    size_t buf_pos = offset;
    bool compressed = false;
    
    while (buf_pos < buffer_size && pos < name_size - 1) {
        uint8_t len = buffer[buf_pos++];
        
        if (len == 0) {
            // End of name
            break;
        } else if ((len & 0xC0) == 0xC0) {
            // Compression pointer
            if (buf_pos >= buffer_size) {
                return 0;
            }
            uint16_t pointer = ((len & 0x3F) << 8) | buffer[buf_pos++];
            if (!compressed) {
                compressed = true;
                buf_pos = pointer;
            } else {
                return 0;  // Invalid compression
            }
        } else {
            // Label
            if (len > 63 || buf_pos + len > buffer_size || pos + len + 1 > name_size) {
                return 0;
            }
            if (pos > 0) {
                name[pos++] = '.';
            }
            memcpy(name + pos, buffer + buf_pos, len);
            pos += len;
            buf_pos += len;
        }
    }
    
    name[pos] = '\0';
    return compressed ? buf_pos : buf_pos;
}

/**
 * Initialize DNS resolver
 */
error_code_t dns_init(void) {
    if (dns_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing DNS resolver...\n");
    
    dns_state.nameserver_ip = DNS_DEFAULT_NAMESERVER;
    dns_state.next_id = 1;
    spinlock_init(&dns_state.lock);
    dns_state.initialized = true;
    
    kinfo("DNS resolver initialized (nameserver: %u.%u.%u.%u)\n",
          (dns_state.nameserver_ip >> 24) & 0xFF,
          (dns_state.nameserver_ip >> 16) & 0xFF,
          (dns_state.nameserver_ip >> 8) & 0xFF,
          dns_state.nameserver_ip & 0xFF);
    
    return ERR_OK;
}

/**
 * Set nameserver IP
 */
error_code_t dns_set_nameserver(uint32_t nameserver_ip) {
    if (!dns_state.initialized) {
        dns_init();
    }
    
    spinlock_lock(&dns_state.lock);
    dns_state.nameserver_ip = nameserver_ip;
    spinlock_unlock(&dns_state.lock);
    
    kinfo("DNS nameserver set to %u.%u.%u.%u\n",
          (nameserver_ip >> 24) & 0xFF,
          (nameserver_ip >> 16) & 0xFF,
          (nameserver_ip >> 8) & 0xFF,
          nameserver_ip & 0xFF);
    
    return ERR_OK;
}

/**
 * Resolve hostname to IPv4 address
 */
error_code_t dns_resolve(const char* hostname, uint32_t* ip_address) {
    if (!hostname || !ip_address) {
        return ERR_INVALID_ARG;
    }
    
    if (!dns_state.initialized) {
        dns_init();
    }
    
    // Create UDP socket
    int sockfd = socket_create(2, SOCK_DGRAM, 0);  // AF_INET, UDP
    if (sockfd < 0) {
        kerror("DNS: Failed to create socket\n");
        return ERR_OUT_OF_MEMORY;
    }
    
    // Bind to random port
    sockaddr_in_t local_addr = {0};
    local_addr.family = 2;  // AF_INET
    local_addr.port = 0;    // Let system choose port
    local_addr.addr = 0;    // Any interface
    
    error_code_t err = socket_bind(sockfd, &local_addr);
    if (err != ERR_OK) {
        kerror("DNS: Failed to bind socket\n");
        socket_close(sockfd);
        return err;
    }
    
    // Build DNS query
    size_t query_size = sizeof(dns_header_t) + 256 + 4;  // Header + name + qtype/qclass
    uint8_t* query = (uint8_t*)kmalloc(query_size);
    if (!query) {
        socket_close(sockfd);
        return ERR_OUT_OF_MEMORY;
    }
    
    dns_header_t* header = (dns_header_t*)query;
    
    spinlock_lock(&dns_state.lock);
    header->id = __builtin_bswap16(dns_state.next_id++);
    spinlock_unlock(&dns_state.lock);
    
    header->flags = __builtin_bswap16(DNS_FLAG_RD);  // Recursion desired
    header->qdcount = __builtin_bswap16(1);
    header->ancount = 0;
    header->nscount = 0;
    header->arcount = 0;
    
    // Encode domain name
    size_t name_len = dns_encode_name(hostname, query + sizeof(dns_header_t), 256);
    if (name_len == 0) {
        kerror("DNS: Failed to encode domain name\n");
        kfree(query);
        socket_close(sockfd);
        return ERR_INVALID_ARG;
    }
    
    // Add question
    size_t q_offset = sizeof(dns_header_t) + name_len;
    uint16_t* qtype = (uint16_t*)(query + q_offset);
    qtype[0] = __builtin_bswap16(DNS_TYPE_A);
    qtype[1] = __builtin_bswap16(DNS_CLASS_IN);
    
    size_t query_len = q_offset + 4;
    
    // Send query
    sockaddr_in_t server_addr = {0};
    server_addr.family = 2;  // AF_INET
    server_addr.port = __builtin_bswap16(DNS_PORT);
    server_addr.addr = dns_state.nameserver_ip;
    
    err = socket_connect(sockfd, &server_addr);
    if (err != ERR_OK) {
        kerror("DNS: Failed to connect to nameserver\n");
        kfree(query);
        socket_close(sockfd);
        return err;
    }
    
    err = socket_send(sockfd, query, query_len, 0);
    if (err != ERR_OK) {
        kerror("DNS: Failed to send query\n");
        kfree(query);
        socket_close(sockfd);
        return err;
    }
    
    // Receive response
    uint8_t* response = (uint8_t*)kmalloc(512);  // Standard DNS packet size
    if (!response) {
        kfree(query);
        socket_close(sockfd);
        return ERR_OUT_OF_MEMORY;
    }
    
    size_t response_len = 512;
    err = socket_recv(sockfd, response, &response_len, 0);
    if (err != ERR_OK || response_len < sizeof(dns_header_t)) {
        kerror("DNS: Failed to receive response\n");
        kfree(query);
        kfree(response);
        socket_close(sockfd);
        return err;
    }
    
    // Parse response
    dns_header_t* resp_header = (dns_header_t*)response;
    uint16_t flags = __builtin_bswap16(resp_header->flags);
    uint16_t rcode = flags & DNS_FLAG_RCODE;
    
    if (rcode != DNS_RCODE_NOERROR) {
        kerror("DNS: Query failed with code %u\n", rcode);
        kfree(query);
        kfree(response);
        socket_close(sockfd);
        return ERR_NOT_FOUND;
    }
    
    uint16_t ancount = __builtin_bswap16(resp_header->ancount);
    if (ancount == 0) {
        kerror("DNS: No answers in response\n");
        kfree(query);
        kfree(response);
        socket_close(sockfd);
        return ERR_NOT_FOUND;
    }
    
    // Skip question section
    size_t offset = sizeof(dns_header_t);
    offset += dns_decode_name(response, response_len, offset, NULL, 0);  // Skip name
    offset += 4;  // Skip qtype and qclass
    
    // Parse answer section
    for (uint16_t i = 0; i < ancount && offset < response_len; i++) {
        // Skip name
        offset += dns_decode_name(response, response_len, offset, NULL, 0);
        
        if (offset + 10 > response_len) {
            break;
        }
        
        uint16_t type = __builtin_bswap16(*(uint16_t*)(response + offset));
        offset += 2;
        uint16_t class = __builtin_bswap16(*(uint16_t*)(response + offset));
        offset += 2;
        uint32_t ttl = __builtin_bswap32(*(uint32_t*)(response + offset));
        offset += 4;
        uint16_t rdlength = __builtin_bswap16(*(uint16_t*)(response + offset));
        offset += 2;
        
        (void)class;
        (void)ttl;
        
        if (type == DNS_TYPE_A && rdlength == 4) {
            if (offset + 4 <= response_len) {
                *ip_address = *(uint32_t*)(response + offset);
                kinfo("DNS: Resolved %s to %u.%u.%u.%u\n", hostname,
                      (*ip_address >> 24) & 0xFF,
                      (*ip_address >> 16) & 0xFF,
                      (*ip_address >> 8) & 0xFF,
                      *ip_address & 0xFF);
                kfree(query);
                kfree(response);
                socket_close(sockfd);
                return ERR_OK;
            }
        }
        
        offset += rdlength;
    }
    
    kerror("DNS: No A record found in response\n");
    kfree(query);
    kfree(response);
    socket_close(sockfd);
    return ERR_NOT_FOUND;
}

/**
 * Resolve hostname to IPv6 address (placeholder)
 */
error_code_t dns_resolve_ipv6(const char* hostname, uint8_t* ipv6_address) {
    (void)hostname;
    (void)ipv6_address;
    // TODO: Implement IPv6 DNS resolution
    return ERR_NOT_IMPLEMENTED;
}

