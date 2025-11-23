#ifndef _SCARLETTOS_NETWORK_H
#define _SCARLETTOS_NETWORK_H

#include <scarlettos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// Socket domains
#define AF_UNSPEC   0
#define AF_UNIX     1
#define AF_INET     2
#define AF_INET6    10

// Socket types
#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOCK_RAW    3

// Protocols
#define IPPROTO_IP   0
#define IPPROTO_TCP  6
#define IPPROTO_UDP  17

typedef uint32_t socklen_t;
typedef uint16_t sa_family_t;

struct sockaddr {
    sa_family_t sa_family;
    char        sa_data[14];
};

struct sockaddr_in {
    sa_family_t    sin_family;
    uint16_t       sin_port;
    struct in_addr sin_addr;
    char           sin_zero[8];
};

struct in_addr {
    uint32_t s_addr;
};

// Socket operations
int sc_socket(int domain, int type, int protocol);
int sc_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int sc_listen(int sockfd, int backlog);
int sc_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int sc_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t sc_send(int sockfd, const void *buf, size_t len, int flags);
ssize_t sc_recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sc_sendto(int sockfd, const void *buf, size_t len, int flags,
                  const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t sc_recvfrom(int sockfd, void *buf, size_t len, int flags,
                    struct sockaddr *src_addr, socklen_t *addrlen);
int sc_shutdown(int sockfd, int how);

#ifdef __cplusplus
}
#endif

#endif // _SCARLETTOS_NETWORK_H
