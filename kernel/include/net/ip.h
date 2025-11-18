/**
 * @file ip.h
 * @brief IP protocol definitions
 */

#ifndef KERNEL_NET_IP_H
#define KERNEL_NET_IP_H

#include "../types.h"
#include "../errors.h"

// IP protocol numbers
#define IP_PROTOCOL_ICMP 1
#define IP_PROTOCOL_TCP  6
#define IP_PROTOCOL_UDP  17

// IP header structure
typedef struct {
    uint8_t version_ihl;      // Version (4 bits) + IHL (4 bits)
    uint8_t tos;              // Type of Service
    uint16_t total_length;    // Total length
    uint16_t identification;
    uint16_t flags_fragment;  // Flags (3 bits) + Fragment offset (13 bits)
    uint8_t ttl;              // Time to Live
    uint8_t protocol;         // Protocol
    uint16_t checksum;        // Header checksum
    uint32_t src_ip;          // Source IP address
    uint32_t dest_ip;         // Destination IP address
    uint8_t data[];
} __attribute__((packed)) ip_header_t;

// IP functions
error_code_t ip_send(uint32_t dest_ip, uint8_t protocol, void* data, size_t len);
error_code_t ip_receive(void* buffer, size_t* len, uint32_t* src_ip, uint8_t* protocol);
uint16_t ip_checksum(ip_header_t* header);

#endif // KERNEL_NET_IP_H

