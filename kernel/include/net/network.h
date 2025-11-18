/**
 * @file network.h
 * @brief Network stack interface
 */

#ifndef KERNEL_NET_NETWORK_H
#define KERNEL_NET_NETWORK_H

#include "../types.h"
#include "../errors.h"

// Network device types
typedef enum {
    NET_DEVICE_TYPE_ETHERNET,
    NET_DEVICE_TYPE_WIFI,
    NET_DEVICE_TYPE_LOOPBACK
} net_device_type_t;

// Network device structure
typedef struct net_device {
    char name[16];
    net_device_type_t type;
    uint8_t mac_address[6];
    uint32_t ip_address;
    uint32_t netmask;
    uint32_t gateway;
    bool up;
    void* driver_data;
    
    // Driver functions
    error_code_t (*send_packet)(struct net_device* dev, void* data, size_t len);
    error_code_t (*receive_packet)(struct net_device* dev, void* buffer, size_t* len);
    
    struct net_device* next;
} net_device_t;

// Network functions
error_code_t network_init(void);
error_code_t network_register_device(net_device_t* device);
net_device_t* network_find_device(const char* name);
error_code_t network_set_ip(net_device_t* device, uint32_t ip, uint32_t netmask, uint32_t gateway);

#endif // KERNEL_NET_NETWORK_H

