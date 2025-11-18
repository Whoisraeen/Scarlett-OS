/**
 * @file socket.h
 * @brief Socket API interface
 */

#ifndef KERNEL_NET_SOCKET_H
#define KERNEL_NET_SOCKET_H

#include "../types.h"
#include "../errors.h"

// Socket types
#define SOCK_STREAM 1  // TCP
#define SOCK_DGRAM  2  // UDP
#define SOCK_RAW    3  // Raw IP

// Socket address structure
typedef struct {
    uint16_t family;      // Address family (AF_INET = 2)
    uint16_t port;        // Port number (network byte order)
    uint32_t addr;        // IP address (network byte order)
    uint8_t zero[8];      // Padding
} __attribute__((packed)) sockaddr_in_t;

// Socket structure
typedef struct socket {
    int type;              // Socket type (SOCK_STREAM, SOCK_DGRAM)
    uint32_t local_ip;     // Local IP address
    uint16_t local_port;   // Local port
    uint32_t remote_ip;    // Remote IP address
    uint16_t remote_port;  // Remote port
    bool bound;            // Is socket bound?
    bool connected;        // Is socket connected?
    void* data;            // Socket-specific data
    struct socket* next;
} socket_t;

// Socket functions
error_code_t socket_init(void);
int socket_create(int domain, int type, int protocol);
error_code_t socket_bind(int sockfd, sockaddr_in_t* addr);
error_code_t socket_listen(int sockfd, int backlog);
error_code_t socket_connect(int sockfd, sockaddr_in_t* addr);
int socket_accept(int sockfd, sockaddr_in_t* addr);
error_code_t socket_send(int sockfd, void* data, size_t len, int flags);
error_code_t socket_recv(int sockfd, void* buffer, size_t* len, int flags);
error_code_t socket_close(int sockfd);
error_code_t socket_setsockopt(int sockfd, int level, int optname, void* optval, size_t optlen);
error_code_t socket_getsockopt(int sockfd, int level, int optname, void* optval, size_t* optlen);

#endif // KERNEL_NET_SOCKET_H

