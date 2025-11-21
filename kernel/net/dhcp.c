/**
 * @file dhcp.c
 * @brief DHCP client implementation
 */

#include "../include/net/dhcp.h"
#include "../include/net/udp.h"
#include "../include/net/ip.h"
#include "../include/net/socket.h"
#include "../include/net/network.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"
#include "../include/sync/spinlock.h"

// DHCP state
static struct {
    uint32_t next_xid;
    spinlock_t lock;
    bool initialized;
} dhcp_state = {0};

/**
 * Initialize DHCP client
 */
error_code_t dhcp_init(void) {
    if (dhcp_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing DHCP client...\n");
    
    dhcp_state.next_xid = 1;
    spinlock_init(&dhcp_state.lock);
    dhcp_state.initialized = true;
    
    kinfo("DHCP client initialized\n");
    return ERR_OK;
}

/**
 * Add DHCP option to message
 */
static size_t dhcp_add_option(uint8_t* options, size_t offset, uint8_t code, const void* data, size_t data_len) {
    if (offset + 2 + data_len >= 312) {
        return offset;  // No space
    }
    
    options[offset++] = code;
    options[offset++] = (uint8_t)data_len;
    if (data && data_len > 0) {
        memcpy(options + offset, data, data_len);
        offset += data_len;
    }
    
    return offset;
}

/**
 * Request DHCP configuration
 */
error_code_t dhcp_request_config(net_device_t* device, dhcp_config_t* config) {
    if (!device || !config) {
        return ERR_INVALID_ARG;
    }
    
    if (!dhcp_state.initialized) {
        dhcp_init();
    }
    
    kinfo("DHCP: Requesting configuration for device %s\n", device->name);
    
    // Create DHCP DISCOVER message
    dhcp_message_t* discover = (dhcp_message_t*)kmalloc(sizeof(dhcp_message_t));
    if (!discover) {
        return ERR_OUT_OF_MEMORY;
    }
    
    memset(discover, 0, sizeof(dhcp_message_t));
    
    spinlock_lock(&dhcp_state.lock);
    discover->xid = dhcp_state.next_xid++;
    spinlock_unlock(&dhcp_state.lock);
    
    discover->op = 1;  // BOOTREQUEST
    discover->htype = 1;  // Ethernet
    discover->hlen = 6;   // MAC address length
    discover->hops = 0;
    discover->secs = 0;
    discover->flags = 0x8000;  // Broadcast flag
    
    // Client hardware address
    memcpy(discover->chaddr, device->mac_address, 6);
    
    // Magic cookie (99.130.83.99)
    discover->options[0] = 99;
    discover->options[1] = 130;
    discover->options[2] = 83;
    discover->options[3] = 99;
    
    // DHCP message type: DISCOVER
    size_t opt_offset = 4;
    uint8_t msg_type = DHCP_DISCOVER;
    opt_offset = dhcp_add_option(discover->options, opt_offset, DHCP_OPT_MESSAGE_TYPE, &msg_type, 1);
    
    // Parameter request list
    uint8_t param_list[] = {DHCP_OPT_SUBNET_MASK, DHCP_OPT_ROUTER, DHCP_OPT_DNS_SERVER};
    opt_offset = dhcp_add_option(discover->options, opt_offset, DHCP_OPT_PARAMETER_REQUEST, param_list, sizeof(param_list));
    
    // End option
    discover->options[opt_offset++] = DHCP_OPT_END;
    
    // Send DISCOVER to broadcast
    uint32_t broadcast_ip = 0xFFFFFFFF;  // 255.255.255.255
    error_code_t err = udp_send(broadcast_ip, DHCP_SERVER_PORT, DHCP_CLIENT_PORT, discover, sizeof(dhcp_message_t));
    
    if (err != ERR_OK) {
        kerror("DHCP: Failed to send DISCOVER\n");
        kfree(discover);
        return err;
    }
    
    kinfo("DHCP: DISCOVER sent, waiting for OFFER...\n");
    
    // Create UDP socket for receiving
    int sockfd = socket_create(2, SOCK_DGRAM, 0);  // AF_INET, UDP
    if (sockfd < 0) {
        kerror("DHCP: Failed to create socket\n");
        kfree(discover);
        return ERR_OUT_OF_MEMORY;
    }
    
    // Bind to DHCP client port
    sockaddr_in_t local_addr = {0};
    local_addr.family = 2;  // AF_INET
    local_addr.port = __builtin_bswap16(DHCP_CLIENT_PORT);
    local_addr.addr = 0;    // Any interface
    
    error_code_t err = socket_bind(sockfd, &local_addr);
    if (err != ERR_OK) {
        kerror("DHCP: Failed to bind socket\n");
        kfree(discover);
        socket_close(sockfd);
        return err;
    }
    
    // Wait for OFFER with timeout (retry up to 3 times)
    uint8_t* response = (uint8_t*)kmalloc(sizeof(dhcp_message_t));
    if (!response) {
        kfree(discover);
        socket_close(sockfd);
        return ERR_OUT_OF_MEMORY;
    }
    
    bool received = false;
    for (int retry = 0; retry < 3; retry++) {
        size_t response_len = sizeof(dhcp_message_t);
        err = socket_recv(sockfd, response, &response_len, 0);
        
        if (err == ERR_OK && response_len >= sizeof(dhcp_message_t)) {
            dhcp_message_t* dhcp_resp = (dhcp_message_t*)response;
            
            // Check if it's a DHCP OFFER
            if (dhcp_resp->op == 2) {  // BOOTREPLY
                // Parse options to find message type
                uint8_t* options = dhcp_resp->options;
                uint8_t msg_type = 0;
                
                // Skip magic cookie
                size_t opt_offset = 4;
                while (opt_offset < sizeof(dhcp_resp->options) - 2) {
                    uint8_t opt_code = options[opt_offset];
                    if (opt_code == DHCP_OPT_END) {
                        break;
                    }
                    if (opt_code == DHCP_OPT_PAD) {
                        opt_offset++;
                        continue;
                    }
                    if (opt_code == DHCP_OPT_MESSAGE_TYPE) {
                        msg_type = options[opt_offset + 2];
                        break;
                    }
                    uint8_t opt_len = options[opt_offset + 1];
                    opt_offset += 2 + opt_len;
                }
                
                if (msg_type == DHCP_OFFER) {
                    kinfo("DHCP: Received OFFER\n");
                    received = true;
                    break;
                }
            }
        }
        
        // Wait a bit before retry
        for (volatile uint32_t delay = 0; delay < 1000000; delay++);
    }
    
    kfree(discover);
    
    if (!received) {
        kerror("DHCP: No OFFER received after retries\n");
        kfree(response);
        socket_close(sockfd);
        return ERR_TIMEOUT;
    }
    
    // Parse OFFER and send REQUEST
    dhcp_message_t* offer = (dhcp_message_t*)response;
    uint32_t server_ip = offer->siaddr;
    uint32_t offered_ip = offer->yiaddr;
    
    kinfo("DHCP: OFFER received - IP: %u.%u.%u.%u, Server: %u.%u.%u.%u\n",
          (offered_ip >> 24) & 0xFF, (offered_ip >> 16) & 0xFF,
          (offered_ip >> 8) & 0xFF, offered_ip & 0xFF,
          (server_ip >> 24) & 0xFF, (server_ip >> 16) & 0xFF,
          (server_ip >> 8) & 0xFF, server_ip & 0xFF);
    
    // Build REQUEST message
    dhcp_message_t* request = (dhcp_message_t*)kmalloc(sizeof(dhcp_message_t));
    if (!request) {
        kfree(response);
        socket_close(sockfd);
        return ERR_OUT_OF_MEMORY;
    }
    
    memset(request, 0, sizeof(dhcp_message_t));
    
    spinlock_lock(&dhcp_state.lock);
    request->xid = discover->xid;  // Use same transaction ID
    spinlock_unlock(&dhcp_state.lock);
    
    request->op = 1;  // BOOTREQUEST
    request->htype = 1;
    request->hlen = 6;
    request->hops = 0;
    request->secs = 0;
    request->flags = 0x8000;  // Broadcast
    
    memcpy(request->chaddr, device->mac_address, 6);
    
    // Magic cookie
    request->options[0] = 99;
    request->options[1] = 130;
    request->options[2] = 83;
    request->options[3] = 99;
    
    // DHCP message type: REQUEST
    size_t opt_offset = 4;
    uint8_t msg_type = DHCP_REQUEST;
    opt_offset = dhcp_add_option(request->options, opt_offset, DHCP_OPT_MESSAGE_TYPE, &msg_type, 1);
    
    // Requested IP
    opt_offset = dhcp_add_option(request->options, opt_offset, DHCP_OPT_REQUESTED_IP, &offered_ip, 4);
    
    // Server ID
    opt_offset = dhcp_add_option(request->options, opt_offset, DHCP_OPT_SERVER_ID, &server_ip, 4);
    
    // Parameter request list
    uint8_t param_list[] = {DHCP_OPT_SUBNET_MASK, DHCP_OPT_ROUTER, DHCP_OPT_DNS_SERVER};
    opt_offset = dhcp_add_option(request->options, opt_offset, DHCP_OPT_PARAMETER_REQUEST, param_list, sizeof(param_list));
    
    // End option
    request->options[opt_offset++] = DHCP_OPT_END;
    
    // Send REQUEST
    err = udp_send(broadcast_ip, DHCP_SERVER_PORT, DHCP_CLIENT_PORT, request, sizeof(dhcp_message_t));
    kfree(request);
    
    if (err != ERR_OK) {
        kerror("DHCP: Failed to send REQUEST\n");
        kfree(response);
        socket_close(sockfd);
        return err;
    }
    
    kinfo("DHCP: REQUEST sent, waiting for ACK...\n");
    
    // Wait for ACK
    received = false;
    for (int retry = 0; retry < 3; retry++) {
        size_t ack_len = sizeof(dhcp_message_t);
        err = socket_recv(sockfd, response, &ack_len, 0);
        
        if (err == ERR_OK && ack_len >= sizeof(dhcp_message_t)) {
            dhcp_message_t* dhcp_ack = (dhcp_message_t*)response;
            
            if (dhcp_ack->op == 2) {  // BOOTREPLY
                // Parse options
                uint8_t* options = dhcp_ack->options;
                uint8_t msg_type = 0;
                
                size_t opt_offset = 4;
                while (opt_offset < sizeof(dhcp_ack->options) - 2) {
                    uint8_t opt_code = options[opt_offset];
                    if (opt_code == DHCP_OPT_END) {
                        break;
                    }
                    if (opt_code == DHCP_OPT_PAD) {
                        opt_offset++;
                        continue;
                    }
                    if (opt_code == DHCP_OPT_MESSAGE_TYPE) {
                        msg_type = options[opt_offset + 2];
                        break;
                    }
                    uint8_t opt_len = options[opt_offset + 1];
                    opt_offset += 2 + opt_len;
                }
                
                if (msg_type == DHCP_ACK) {
                    kinfo("DHCP: Received ACK\n");
                    received = true;
                    break;
                } else if (msg_type == DHCP_NAK) {
                    kerror("DHCP: Received NAK\n");
                    kfree(response);
                    socket_close(sockfd);
                    return ERR_FAILED;
                }
            }
        }
        
        // Wait before retry
        for (volatile uint32_t delay = 0; delay < 1000000; delay++);
    }
    
    socket_close(sockfd);
    
    if (!received) {
        kerror("DHCP: No ACK received\n");
        kfree(response);
        return ERR_TIMEOUT;
    }
    
    // Parse ACK and extract configuration
    dhcp_message_t* ack = (dhcp_message_t*)response;
    
    config->ip_address = ack->yiaddr;
    config->subnet_mask = 0xFFFFFF00;  // Default /24
    config->gateway = ack->giaddr;
    config->dns_server = 0x08080808;  // Default to Google DNS
    config->lease_time = 86400;  // Default 24 hours
    config->configured = true;
    
    // Parse options for subnet mask, router, DNS, lease time
    uint8_t* options = ack->options;
    size_t opt_offset = 4;
    while (opt_offset < sizeof(ack->options) - 2) {
        uint8_t opt_code = options[opt_offset];
        if (opt_code == DHCP_OPT_END) {
            break;
        }
        if (opt_code == DHCP_OPT_PAD) {
            opt_offset++;
            continue;
        }
        
        uint8_t opt_len = options[opt_offset + 1];
        if (opt_len > 0 && opt_offset + 2 + opt_len <= sizeof(ack->options)) {
            switch (opt_code) {
                case DHCP_OPT_SUBNET_MASK:
                    if (opt_len == 4) {
                        config->subnet_mask = *(uint32_t*)(options + opt_offset + 2);
                    }
                    break;
                case DHCP_OPT_ROUTER:
                    if (opt_len >= 4) {
                        config->gateway = *(uint32_t*)(options + opt_offset + 2);
                    }
                    break;
                case DHCP_OPT_DNS_SERVER:
                    if (opt_len >= 4) {
                        config->dns_server = *(uint32_t*)(options + opt_offset + 2);
                    }
                    break;
                case DHCP_OPT_LEASE_TIME:
                    if (opt_len == 4) {
                        config->lease_time = __builtin_bswap32(*(uint32_t*)(options + opt_offset + 2));
                    }
                    break;
            }
        }
        opt_offset += 2 + opt_len;
    }
    
    kfree(response);
    
    kinfo("DHCP: Configuration received - IP: %u.%u.%u.%u/%u, Gateway: %u.%u.%u.%u, DNS: %u.%u.%u.%u\n",
          (config->ip_address >> 24) & 0xFF, (config->ip_address >> 16) & 0xFF,
          (config->ip_address >> 8) & 0xFF, config->ip_address & 0xFF,
          __builtin_clz(~config->subnet_mask),
          (config->gateway >> 24) & 0xFF, (config->gateway >> 16) & 0xFF,
          (config->gateway >> 8) & 0xFF, config->gateway & 0xFF,
          (config->dns_server >> 24) & 0xFF, (config->dns_server >> 16) & 0xFF,
          (config->dns_server >> 8) & 0xFF, config->dns_server & 0xFF);
    
    return ERR_OK;
}

/**
 * Release DHCP configuration
 */
error_code_t dhcp_release_config(net_device_t* device) {
    if (!device) {
        return ERR_INVALID_ARG;
    }
    
    if (!dhcp_state.initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    kinfo("DHCP: Releasing configuration for device %s\n", device->name);
    
    // TODO: Send DHCP RELEASE message
    return ERR_NOT_IMPLEMENTED;
}

