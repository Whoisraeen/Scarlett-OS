/**
 * @file tcp.c
 * @brief TCP protocol implementation
 */

#include "../include/net/tcp.h"
#include "../include/net/ip.h"
#include "../include/net/network.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"
#include "../include/sync/spinlock.h"

// TCP flag macros
#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

// TCP connections
#define MAX_TCP_CONNECTIONS 32

static struct {
    tcp_connection_t* connections[MAX_TCP_CONNECTIONS];
    uint32_t next_seq_num;
    spinlock_t lock;
    bool initialized;
} tcp_state = {0};

// Forward declarations
extern error_code_t ip_send(uint32_t dest_ip, uint8_t protocol, void* data, size_t len);
extern net_device_t* network_find_device(const char* name);

/**
 * Calculate TCP checksum (pseudo-header + header + data)
 */
static uint16_t tcp_checksum(tcp_header_t* header, size_t tcp_len, uint32_t src_ip, uint32_t dest_ip) {
    // Pseudo-header for checksum
    struct {
        uint32_t src_ip;
        uint32_t dest_ip;
        uint8_t zero;
        uint8_t protocol;
        uint16_t tcp_length;
    } __attribute__((packed)) pseudo_header = {
        .src_ip = src_ip,
        .dest_ip = dest_ip,
        .zero = 0,
        .protocol = IP_PROTOCOL_TCP,
        .tcp_length = __builtin_bswap16(tcp_len)
    };
    
    uint32_t sum = 0;
    
    // Add pseudo-header
    uint16_t* words = (uint16_t*)&pseudo_header;
    for (int i = 0; i < 6; i++) {
        sum += __builtin_bswap16(words[i]);
    }
    
    // Add TCP header and data
    words = (uint16_t*)header;
    size_t word_count = tcp_len / 2;
    for (size_t i = 0; i < word_count; i++) {
        sum += __builtin_bswap16(words[i]);
    }
    
    // Add padding byte if odd length
    if (tcp_len % 2) {
        sum += ((uint8_t*)header)[tcp_len - 1] << 8;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return __builtin_bswap16(~sum);
}

/**
 * Initialize TCP
 */
error_code_t tcp_init(void) {
    if (tcp_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing TCP...\n");
    
    memset(tcp_state.connections, 0, sizeof(tcp_state.connections));
    tcp_state.next_seq_num = 1;  // Start at 1
    spinlock_init(&tcp_state.lock);
    tcp_state.initialized = true;
    
    kinfo("TCP initialized\n");
    return ERR_OK;
}

/**
 * Find TCP connection
 */
static tcp_connection_t* tcp_find_connection(uint32_t local_ip, uint16_t local_port, 
                                             uint32_t remote_ip, uint16_t remote_port) {
    spinlock_lock(&tcp_state.lock);
    
    for (int i = 0; i < MAX_TCP_CONNECTIONS; i++) {
        tcp_connection_t* conn = tcp_state.connections[i];
        if (conn &&
            conn->local_ip == local_ip &&
            conn->local_port == local_port &&
            conn->remote_ip == remote_ip &&
            conn->remote_port == remote_port) {
            spinlock_unlock(&tcp_state.lock);
            return conn;
        }
    }
    
    spinlock_unlock(&tcp_state.lock);
    return NULL;
}

/**
 * Create TCP connection
 */
tcp_connection_t* tcp_create_connection(uint32_t local_ip, uint16_t local_port, 
                                       uint32_t remote_ip, uint16_t remote_port) {
    if (!tcp_state.initialized) {
        tcp_init();
    }
    
    // Check if connection already exists
    tcp_connection_t* existing = tcp_find_connection(local_ip, local_port, remote_ip, remote_port);
    if (existing) {
        return existing;
    }
    
    // Allocate connection
    tcp_connection_t* conn = (tcp_connection_t*)kmalloc(sizeof(tcp_connection_t));
    if (!conn) {
        return NULL;
    }
    
    memset(conn, 0, sizeof(tcp_connection_t));
    conn->local_ip = local_ip;
    conn->local_port = local_port;
    conn->remote_ip = remote_ip;
    conn->remote_port = remote_port;
    conn->state = TCP_STATE_CLOSED;
    conn->seq_num = tcp_state.next_seq_num++;
    conn->ack_num = 0;
    conn->window_size = 4096;
    conn->receive_buffer_size = 4096;
    conn->receive_buffer = kmalloc(conn->receive_buffer_size);
    conn->receive_buffer_pos = 0;
    
    // Add to connection list
    spinlock_lock(&tcp_state.lock);
    for (int i = 0; i < MAX_TCP_CONNECTIONS; i++) {
        if (!tcp_state.connections[i]) {
            tcp_state.connections[i] = conn;
            break;
        }
    }
    spinlock_unlock(&tcp_state.lock);
    
    return conn;
}

/**
 * Send TCP packet
 */
error_code_t tcp_send_packet(tcp_connection_t* conn, uint8_t flags, void* data, size_t data_len) {
    if (!conn) {
        return ERR_INVALID_ARG;
    }
    
    size_t header_size = sizeof(tcp_header_t);
    size_t packet_size = header_size + data_len;
    
    tcp_header_t* tcp = (tcp_header_t*)kmalloc(packet_size);
    if (!tcp) {
        return ERR_OUT_OF_MEMORY;
    }
    
    memset(tcp, 0, packet_size);
    
    // Build TCP header
    tcp->src_port = conn->local_port;
    tcp->dest_port = conn->remote_port;
    tcp->seq_number = __builtin_bswap32(conn->seq_num);
    tcp->ack_number = __builtin_bswap32(conn->ack_num);
    tcp->data_offset = (header_size / 4) << 4;  // Data offset in 32-bit words
    tcp->flags = flags;
    tcp->window_size = __builtin_bswap16(conn->window_size);
    tcp->checksum = 0;
    tcp->urgent_ptr = 0;
    
    // Copy data
    if (data && data_len > 0) {
        memcpy(tcp->options + 0, data, data_len);
    }
    
    // Calculate checksum
    tcp->checksum = tcp_checksum(tcp, packet_size, conn->local_ip, conn->remote_ip);
    
    // Send via IP
    error_code_t err = ip_send(conn->remote_ip, IP_PROTOCOL_TCP, tcp, packet_size);
    
    // Update sequence number
    if (data_len > 0 || (flags & TCP_FLAG_SYN)) {
        conn->seq_num += data_len + ((flags & TCP_FLAG_SYN) ? 1 : 0);
    }
    
    kfree(tcp);
    return err;
}

/**
 * Send data on TCP connection
 */
error_code_t tcp_send(tcp_connection_t* conn, void* data, size_t len) {
    if (!conn || !data || len == 0) {
        return ERR_INVALID_ARG;
    }
    
    if (conn->state != TCP_STATE_ESTABLISHED) {
        return ERR_INVALID_STATE;
    }
    
    return tcp_send_packet(conn, TCP_FLAG_ACK | TCP_FLAG_PSH, data, len);
}

/**
 * Receive data from TCP connection
 */
error_code_t tcp_receive(tcp_connection_t* conn, void* buffer, size_t* len) {
    if (!conn || !buffer || !len) {
        return ERR_INVALID_ARG;
    }
    
    if (conn->state != TCP_STATE_ESTABLISHED) {
        return ERR_INVALID_STATE;
    }
    
    if (conn->receive_buffer_pos == 0) {
        return ERR_NOT_FOUND;  // No data available
    }
    
    size_t copy_len = (*len < conn->receive_buffer_pos) ? *len : conn->receive_buffer_pos;
    memcpy(buffer, conn->receive_buffer, copy_len);
    
    // Shift remaining data
    if (conn->receive_buffer_pos > copy_len) {
        memmove(conn->receive_buffer, 
                (uint8_t*)conn->receive_buffer + copy_len,
                conn->receive_buffer_pos - copy_len);
    }
    
    conn->receive_buffer_pos -= copy_len;
    *len = copy_len;
    
    return ERR_OK;
}

/**
 * Handle incoming TCP packet
 */
error_code_t tcp_handle_packet(void* buffer, size_t len, uint32_t src_ip) {
    if (!tcp_state.initialized || !buffer || len < sizeof(tcp_header_t)) {
        return ERR_INVALID_ARG;
    }
    
    tcp_header_t* tcp = (tcp_header_t*)buffer;
    
    uint16_t src_port = __builtin_bswap16(tcp->src_port);
    uint16_t dest_port = __builtin_bswap16(tcp->dest_port);
    uint32_t seq_num = __builtin_bswap32(tcp->seq_number);
    uint32_t ack_num = __builtin_bswap32(tcp->ack_number);
    uint8_t flags = tcp->flags;
    
    // Find connection
    net_device_t* device = network_find_device("eth0");
    if (!device || !device->up) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    tcp_connection_t* conn = tcp_find_connection(device->ip_address, dest_port, src_ip, src_port);
    
    if (!conn && (flags & TCP_FLAG_SYN)) {
        // New connection - SYN received
        // Check for listener
        tcp_connection_t* listener = tcp_find_connection(0, dest_port, 0, 0); // Listener has 0 remote IP/Port
        // Actually listeners might have local_ip set or 0 (INADDR_ANY).
        // Let's search for listener with matching local port and state LISTEN
        if (!listener) {
            spinlock_lock(&tcp_state.lock);
            for (int i = 0; i < MAX_TCP_CONNECTIONS; i++) {
                tcp_connection_t* c = tcp_state.connections[i];
                if (c && c->state == TCP_STATE_LISTEN && c->local_port == dest_port) {
                    listener = c;
                    break;
                }
            }
            spinlock_unlock(&tcp_state.lock);
        }
        
        if (listener) {
            // Create connection in SYN_RECEIVED state
            conn = tcp_create_connection(device->ip_address, dest_port, src_ip, src_port);
            if (conn) {
                conn->state = TCP_STATE_SYN_RECEIVED;
                conn->ack_num = seq_num + 1;
                
                // Send SYN-ACK
                tcp_send_packet(conn, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
                
                // For simplified TCP, assume established after SYN-ACK sent (skip 3-way handshake completion wait)
                // In real TCP, we wait for ACK.
                // But for now, let's just mark it established and queue it.
                conn->state = TCP_STATE_ESTABLISHED;
                
                // Add to listener's pending queue
                spinlock_lock(&tcp_state.lock);
                if (listener->pending_tail) {
                    listener->pending_tail->next_pending = (struct tcp_connection*)conn;
                    listener->pending_tail = (struct tcp_connection*)conn;
                } else {
                    listener->pending_head = (struct tcp_connection*)conn;
                    listener->pending_tail = (struct tcp_connection*)conn;
                }
                conn->next_pending = NULL;
                spinlock_unlock(&tcp_state.lock);
            }
            return ERR_OK;
        }
        
        // No listener, reject
        // Send RST?
        return ERR_NOT_FOUND;
    }
    
    if (!conn) {
        return ERR_NOT_FOUND;
    }
    
    // Update acknowledgment number
    if (flags & TCP_FLAG_ACK) {
        if (ack_num > conn->seq_num) {
            // Data acknowledged
        }
    }
    
    // Handle different flags
    if (flags & TCP_FLAG_FIN) {
        // Connection close
        if (conn->state == TCP_STATE_ESTABLISHED) {
            conn->state = TCP_STATE_CLOSE_WAIT;
            conn->ack_num = seq_num + 1;
            // Send ACK
            tcp_send_packet(conn, TCP_FLAG_ACK, NULL, 0);
            // Send FIN-ACK
            tcp_send_packet(conn, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
            conn->state = TCP_STATE_LAST_ACK;
        }
    }
    
    if (flags & TCP_FLAG_ACK && conn->state == TCP_STATE_LAST_ACK) {
        // FIN acknowledged
        tcp_close(conn);
        return ERR_OK;
    }
    
    // Extract data
    size_t data_offset = ((tcp->data_offset >> 4) & 0x0F) * 4;
    if (data_offset < sizeof(tcp_header_t)) {
        data_offset = sizeof(tcp_header_t);
    }
    
    if (len > data_offset) {
        size_t data_len = len - data_offset;
        void* data = (uint8_t*)buffer + data_offset;
        
        // Store in receive buffer
        if (conn->receive_buffer_pos + data_len <= conn->receive_buffer_size) {
            memcpy((uint8_t*)conn->receive_buffer + conn->receive_buffer_pos, data, data_len);
            conn->receive_buffer_pos += data_len;
            conn->ack_num = seq_num + data_len;
            
            // Send ACK
            tcp_send_packet(conn, TCP_FLAG_ACK, NULL, 0);
        }
    }
    
    return ERR_OK;
}

/**
 * Listen on a port
 */
error_code_t tcp_listen(uint16_t port) {
    if (!tcp_state.initialized) {
        tcp_init();
    }
    
    // Check if already listening
    spinlock_lock(&tcp_state.lock);
    for (int i = 0; i < MAX_TCP_CONNECTIONS; i++) {
        tcp_connection_t* c = tcp_state.connections[i];
        if (c && c->state == TCP_STATE_LISTEN && c->local_port == port) {
            spinlock_unlock(&tcp_state.lock);
            return ERR_ALREADY_EXISTS;
        }
    }
    spinlock_unlock(&tcp_state.lock);
    
    // Create listener connection
    // Use 0.0.0.0 (INADDR_ANY) for local IP to listen on all interfaces
    // But tcp_create_connection takes specific IP.
    // We can use 0 to indicate ANY?
    // Let's use 0.
    tcp_connection_t* conn = tcp_create_connection(0, port, 0, 0);
    if (!conn) {
        return ERR_OUT_OF_MEMORY;
    }
    
    conn->state = TCP_STATE_LISTEN;
    return ERR_OK;
}

/**
 * Accept a connection
 */
tcp_connection_t* tcp_accept(uint16_t port) {
    // Find listener
    tcp_connection_t* listener = NULL;
    spinlock_lock(&tcp_state.lock);
    for (int i = 0; i < MAX_TCP_CONNECTIONS; i++) {
        tcp_connection_t* c = tcp_state.connections[i];
        if (c && c->state == TCP_STATE_LISTEN && c->local_port == port) {
            listener = c;
            break;
        }
    }
    
    if (!listener) {
        spinlock_unlock(&tcp_state.lock);
        return NULL;
    }
    
    // Check pending queue
    if (listener->pending_head) {
        tcp_connection_t* conn = (tcp_connection_t*)listener->pending_head;
        listener->pending_head = conn->next_pending;
        if (!listener->pending_head) {
            listener->pending_tail = NULL;
        }
        conn->next_pending = NULL;
        spinlock_unlock(&tcp_state.lock);
        return conn;
    }
    
    spinlock_unlock(&tcp_state.lock);
    return NULL; // No pending connections (would block in blocking mode)
}

/**
 * Close TCP connection
 */
error_code_t tcp_close(tcp_connection_t* conn) {
    if (!conn) {
        return ERR_INVALID_ARG;
    }
    
    if (conn->state == TCP_STATE_ESTABLISHED) {
        // Send FIN
        tcp_send_packet(conn, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
        conn->state = TCP_STATE_FIN_WAIT_1;
    }
    
    // Remove from connection list
    spinlock_lock(&tcp_state.lock);
    for (int i = 0; i < MAX_TCP_CONNECTIONS; i++) {
        if (tcp_state.connections[i] == conn) {
            tcp_state.connections[i] = NULL;
            break;
        }
    }
    spinlock_unlock(&tcp_state.lock);
    
    if (conn->receive_buffer) {
        kfree(conn->receive_buffer);
    }
    kfree(conn);
    
    return ERR_OK;
}

