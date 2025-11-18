/**
 * @file icmp.h
 * @brief ICMP protocol definitions
 */

#ifndef KERNEL_NET_ICMP_H
#define KERNEL_NET_ICMP_H

#include "../types.h"
#include "../errors.h"

// ICMP message types
#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8
#define ICMP_TYPE_DEST_UNREACH 3
#define ICMP_TYPE_TIME_EXCEEDED 11

// ICMP codes
#define ICMP_CODE_ECHO         0

// ICMP header structure
typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    union {
        struct {
            uint16_t identifier;
            uint16_t sequence;
        } echo;
        uint32_t unused;
    } data;
    uint8_t payload[];
} __attribute__((packed)) icmp_header_t;

// ICMP functions
error_code_t icmp_init(void);
error_code_t icmp_send_echo(uint32_t dest_ip, uint16_t identifier, uint16_t sequence, void* data, size_t len);
error_code_t icmp_handle_packet(void* buffer, size_t len, uint32_t src_ip);

#endif // KERNEL_NET_ICMP_H

