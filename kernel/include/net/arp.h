/**
 * @file arp.h
 * @brief ARP protocol definitions
 */

#ifndef KERNEL_NET_ARP_H
#define KERNEL_NET_ARP_H

#include "../types.h"
#include "../errors.h"
#include "network.h"

// ARP operation codes
#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

// ARP hardware types
#define ARP_HW_TYPE_ETHERNET 1

// ARP protocol types
#define ARP_PROTO_TYPE_IPV4 0x0800

// ARP cache entry
typedef struct {
    uint32_t ip_address;
    uint8_t mac_address[6];
    uint64_t timestamp;  // For cache expiration
    bool valid;
} arp_cache_entry_t;

// ARP packet structure
typedef struct {
    uint16_t hardware_type;
    uint16_t protocol_type;
    uint8_t hardware_size;
    uint8_t protocol_size;
    uint16_t operation;
    uint8_t sender_mac[6];
    uint32_t sender_ip;
    uint8_t target_mac[6];
    uint32_t target_ip;
} __attribute__((packed)) arp_packet_t;

// ARP functions
error_code_t arp_init(void);
error_code_t arp_request(uint32_t ip_address);
error_code_t arp_resolve(uint32_t ip_address, uint8_t* mac_address);
error_code_t arp_handle_packet(net_device_t* device, void* buffer, size_t len);
void arp_update_cache(uint32_t ip_address, uint8_t* mac_address);

#endif // KERNEL_NET_ARP_H

