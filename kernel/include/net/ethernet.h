/**
 * @file ethernet.h
 * @brief Ethernet protocol definitions
 */

#ifndef KERNEL_NET_ETHERNET_H
#define KERNEL_NET_ETHERNET_H

#include "../types.h"
#include "../errors.h"
#include "network.h"

// Ethernet frame structure
#define ETH_HEADER_SIZE 14
#define ETH_MIN_SIZE 60
#define ETH_MAX_SIZE 1514

// Ethernet types
#define ETH_TYPE_IPV4  0x0800
#define ETH_TYPE_ARP   0x0806
#define ETH_TYPE_IPV6  0x86DD
#define ETH_TYPE_ICMP  0x0800  // ICMP is inside IP packets

typedef struct {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t type;
    uint8_t data[];
} __attribute__((packed)) ethernet_frame_t;

// Ethernet protocol handler type
typedef error_code_t (*ethernet_protocol_handler_t)(net_device_t* device, void* packet, size_t len);

// Ethernet functions
error_code_t ethernet_init(void);
error_code_t ethernet_register_protocol(uint16_t type, ethernet_protocol_handler_t handler);
error_code_t ethernet_send(void* data, size_t len, uint8_t* dest_mac, uint16_t type, net_device_t* device);
error_code_t ethernet_receive(net_device_t* device, void* buffer, size_t* len);

#endif // KERNEL_NET_ETHERNET_H

