/**
 * @file socket_options.h
 * @brief Socket options definitions
 */

#ifndef KERNEL_NET_SOCKET_OPTIONS_H
#define KERNEL_NET_SOCKET_OPTIONS_H

#include "../types.h"
#include "../errors.h"

// Socket option levels
#define SOL_SOCKET   1

// Socket options
#define SO_REUSEADDR 2
#define SO_KEEPALIVE 9
#define SO_RCVBUF    8
#define SO_SNDBUF    7

// Getsockopt/Setsockopt functions
error_code_t socket_setsockopt(int sockfd, int level, int optname, void* optval, size_t optlen);
error_code_t socket_getsockopt(int sockfd, int level, int optname, void* optval, size_t* optlen);

#endif // KERNEL_NET_SOCKET_OPTIONS_H

