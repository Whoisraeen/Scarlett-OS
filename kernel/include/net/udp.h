/**
 * @file udp.h
 * @brief UDP protocol definitions
 */

#ifndef KERNEL_NET_UDP_H
#define KERNEL_NET_UDP_H

#include "../types.h"
#include "../errors.h"

// UDP header structure
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
    uint8_t data[];
} __attribute__((packed)) udp_header_t;

// UDP functions
error_code_t udp_send(uint32_t dest_ip, uint16_t dest_port, uint16_t src_port, void* data, size_t len);
error_code_t udp_receive(void* buffer, size_t* len, uint32_t* src_ip, uint16_t* src_port, uint16_t* dest_port);

#endif // KERNEL_NET_UDP_H

