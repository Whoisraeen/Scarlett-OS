/**
 * @file socket.c
 * @brief Socket API implementation
 */

#include "../include/net/socket.h"
#include "../include/net/udp.h"
#include "../include/net/tcp.h"
#include "../include/net/network.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/sync/spinlock.h"
#include "../include/string.h"

// Socket system state
static struct {
    socket_t* sockets;
    int next_fd;
    spinlock_t lock;
    bool initialized;
} socket_state = {0};

/**
 * Initialize socket system
 */
error_code_t socket_init(void) {
    if (socket_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing socket system...\n");
    
    socket_state.sockets = NULL;
    socket_state.next_fd = 3;  // Start after stdin, stdout, stderr
    spinlock_init(&socket_state.lock);
    socket_state.initialized = true;
    
    kinfo("Socket system initialized\n");
    return ERR_OK;
}

/**
 * Find socket by file descriptor
 */
static socket_t* socket_find(int fd) {
    spinlock_lock(&socket_state.lock);
    
    socket_t* sock = socket_state.sockets;
    while (sock) {
        if ((int)(sock - socket_state.sockets) == fd - 3) {
            spinlock_unlock(&socket_state.lock);
            return sock;
        }
        sock = sock->next;
    }
    
    spinlock_unlock(&socket_state.lock);
    return NULL;
}

/**
 * Create a socket
 */
int socket_create(int domain, int type, int protocol) {
    if (!socket_state.initialized) {
        socket_init();
    }
    
    if (domain != 2) {  // AF_INET
        return -1;
    }
    
    socket_t* sock = (socket_t*)kmalloc(sizeof(socket_t));
    if (!sock) {
        return -1;
    }
    
    memset(sock, 0, sizeof(socket_t));
    sock->type = type;
    sock->bound = false;
    sock->connected = false;
    
    spinlock_lock(&socket_state.lock);
    sock->next = socket_state.sockets;
    socket_state.sockets = sock;
    int fd = socket_state.next_fd++;
    spinlock_unlock(&socket_state.lock);
    
    return fd;
}

/**
 * Bind socket to address
 */
error_code_t socket_bind(int sockfd, sockaddr_in_t* addr) {
    socket_t* sock = socket_find(sockfd);
    if (!sock || !addr) {
        return ERR_INVALID_ARG;
    }
    
    sock->local_ip = addr->addr;
    sock->local_port = addr->port;
    sock->bound = true;
    
    return ERR_OK;
}

/**
 * Listen on socket (TCP only)
 */
error_code_t socket_listen(int sockfd, int backlog) {
    socket_t* sock = socket_find(sockfd);
    if (!sock) {
        return ERR_INVALID_ARG;
    }
    
    if (sock->type != SOCK_STREAM) {
        return ERR_NOT_SUPPORTED;
    }
    
    if (!sock->bound) {
        return ERR_INVALID_STATE;
    }
    
    // Call TCP listen
    extern error_code_t tcp_listen(uint16_t port);
    return tcp_listen(sock->local_port);
}

/**
 * Connect socket to remote address
 */
error_code_t socket_connect(int sockfd, sockaddr_in_t* addr) {
    socket_t* sock = socket_find(sockfd);
    if (!sock || !addr) {
        return ERR_INVALID_ARG;
    }
    
    sock->remote_ip = addr->addr;
    sock->remote_port = addr->port;
    
    if (sock->type == SOCK_STREAM) {
        // TCP - establish connection
        net_device_t* device = network_find_device("eth0");
        if (!device || !device->up) {
            return ERR_DEVICE_NOT_FOUND;
        }
        
        extern tcp_connection_t* tcp_create_connection(uint32_t local_ip, uint16_t local_port, uint32_t remote_ip, uint16_t remote_port);
        tcp_connection_t* conn = tcp_create_connection(
            device->ip_address, 
            sock->bound ? sock->local_port : 0,
            sock->remote_ip,
            sock->remote_port
        );
        
        if (!conn) {
            return ERR_OUT_OF_MEMORY;
        }
        
        sock->data = conn;
        
        // Send SYN
        extern error_code_t tcp_send_packet(tcp_connection_t* conn, uint8_t flags, void* data, size_t data_len);
        conn->state = TCP_STATE_SYN_SENT;
        tcp_send_packet(conn, TCP_FLAG_SYN, NULL, 0);
        // Simplified - would wait for SYN-ACK in full implementation
        conn->state = TCP_STATE_ESTABLISHED;
    }
    
    sock->connected = true;
    return ERR_OK;
}

/**
 * Accept connection (TCP only)
 */
int socket_accept(int sockfd, sockaddr_in_t* addr) {
    socket_t* sock = socket_find(sockfd);
    if (!sock) {
        return -1;
    }
    
    if (sock->type != SOCK_STREAM) {
        return -1;
    }
    
    if (!sock->bound) {
        return -1;
    }
    
    // Call TCP accept
    extern tcp_connection_t* tcp_accept(uint16_t port);
    tcp_connection_t* conn = tcp_accept(sock->local_port);
    
    if (!conn) {
        return -1; // Would block
    }
    
    // Create new socket for connection
    int new_fd = socket_create(2, SOCK_STREAM, 0); // AF_INET
    if (new_fd < 0) {
        // Close connection if socket creation fails?
        // tcp_close(conn);
        return -1;
    }
    
    socket_t* new_sock = socket_find(new_fd);
    if (new_sock) {
        new_sock->connected = true;
        new_sock->bound = true;
        new_sock->local_ip = conn->local_ip;
        new_sock->local_port = conn->local_port;
        new_sock->remote_ip = conn->remote_ip;
        new_sock->remote_port = conn->remote_port;
        new_sock->data = conn;
        
        if (addr) {
            addr->family = 2; // AF_INET
            addr->port = conn->remote_port;
            addr->addr = conn->remote_ip;
        }
    }
    
    return new_fd;
}

/**
 * Send data on socket
 */
error_code_t socket_send(int sockfd, void* data, size_t len, int flags __attribute__((unused))) {
    socket_t* sock = socket_find(sockfd);
    if (!sock || !data || len == 0) {
        return ERR_INVALID_ARG;
    }
    
    if (!sock->connected && !sock->bound) {
        return ERR_INVALID_STATE;
    }
    
    switch (sock->type) {
        case SOCK_DGRAM: {
            // UDP
            uint32_t dest_ip = sock->connected ? sock->remote_ip : 0;  // Would need destination
            uint16_t dest_port = sock->connected ? sock->remote_port : 0;
            uint16_t src_port = sock->bound ? sock->local_port : 0;
            
            return udp_send(dest_ip, dest_port, src_port, data, len);
        }
        
        case SOCK_STREAM: {
            // TCP
            if (sock->data) {
                extern error_code_t tcp_send(tcp_connection_t* conn, void* data, size_t len);
                return tcp_send((tcp_connection_t*)sock->data, data, len);
            }
            return ERR_INVALID_STATE;
        }
        
        default:
            return ERR_NOT_SUPPORTED;
    }
}

/**
 * Receive data from socket
 */
error_code_t socket_recv(int sockfd, void* buffer, size_t* len, int flags __attribute__((unused))) {
    socket_t* sock = socket_find(sockfd);
    if (!sock || !buffer || !len) {
        return ERR_INVALID_ARG;
    }
    
    switch (sock->type) {
        case SOCK_DGRAM: {
            // UDP
            uint32_t src_ip;
            uint16_t src_port, dest_port;
            
            error_code_t err = udp_receive(buffer, len, &src_ip, &src_port, &dest_port);
            if (err != ERR_OK) {
                return err;
            }
            
            // Check if port matches
            if (sock->bound && dest_port != sock->local_port) {
                return ERR_NOT_FOUND;
            }
            
            return ERR_OK;
        }
        
        case SOCK_STREAM: {
            // TCP
            if (sock->data) {
                extern error_code_t tcp_receive(tcp_connection_t* conn, void* buffer, size_t* len);
                return tcp_receive((tcp_connection_t*)sock->data, buffer, len);
            }
            return ERR_INVALID_STATE;
        }
        
        default:
            return ERR_NOT_SUPPORTED;
    }
}

/**
 * Close socket
 */
error_code_t socket_close(int sockfd) {
    socket_t* sock = socket_find(sockfd);
    if (!sock) {
        return ERR_INVALID_ARG;
    }
    
    spinlock_lock(&socket_state.lock);
    
    // Remove from list
    if (socket_state.sockets == sock) {
        socket_state.sockets = sock->next;
    } else {
        socket_t* prev = socket_state.sockets;
        while (prev && prev->next != sock) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = sock->next;
        }
    }
    
    spinlock_unlock(&socket_state.lock);
    
    // Close TCP connection if exists
    if (sock->type == SOCK_STREAM && sock->data) {
        extern error_code_t tcp_close(tcp_connection_t* conn);
        tcp_close((tcp_connection_t*)sock->data);
        sock->data = NULL;
    }
    
    kfree(sock);
    
    return ERR_OK;
}

/**
 * Set socket option
 */
error_code_t socket_setsockopt(int sockfd, int level, int optname, void* optval, size_t optlen) {
    socket_t* sock = socket_find(sockfd);
    if (!sock || !optval) {
        return ERR_INVALID_ARG;
    }
    
    if (level != 1) {  // SOL_SOCKET
        return ERR_NOT_SUPPORTED;
    }
    
    switch (optname) {
        case 2: {  // SO_REUSEADDR
            if (optlen != sizeof(int)) {
                return ERR_INVALID_ARG;
            }
            return ERR_OK;
        }
        
        case 9: {  // SO_KEEPALIVE
            if (optlen != sizeof(int)) {
                return ERR_INVALID_ARG;
            }
            return ERR_OK;
        }
        
        default:
            return ERR_NOT_SUPPORTED;
    }
}

/**
 * Get socket option
 */
error_code_t socket_getsockopt(int sockfd, int level, int optname, void* optval, size_t* optlen) {
    socket_t* sock = socket_find(sockfd);
    if (!sock || !optval || !optlen) {
        return ERR_INVALID_ARG;
    }
    
    if (level != 1) {  // SOL_SOCKET
        return ERR_NOT_SUPPORTED;
    }
    
    switch (optname) {
        case 2: {  // SO_REUSEADDR
            if (*optlen < sizeof(int)) {
                return ERR_INVALID_ARG;
            }
            *(int*)optval = 0;
            *optlen = sizeof(int);
            return ERR_OK;
        }
        
        case 9: {  // SO_KEEPALIVE
            if (*optlen < sizeof(int)) {
                return ERR_INVALID_ARG;
            }
            *(int*)optval = 0;
            *optlen = sizeof(int);
            return ERR_OK;
        }
        
        default:
            return ERR_NOT_SUPPORTED;
    }
}
