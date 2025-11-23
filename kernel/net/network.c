/**
 * @file network.c
 * @brief Network stack core implementation
 */

#include "../include/net/network.h"
#include "../include/net/ethernet.h"
#include "../include/net/arp.h"
#include "../include/net/ip.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/sync/spinlock.h"

// Network stack state
static struct {
    net_device_t* devices;
    spinlock_t lock;
    bool initialized;
} network_state = {0};

/**
 * Initialize network stack
 */
error_code_t network_init(void) {
    if (network_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing network stack...\n");
    
    network_state.devices = NULL;
    spinlock_init(&network_state.lock);
    network_state.initialized = true;
    
    // Initialize protocols
    ethernet_init();
    arp_init();
    
    // Register protocol handlers
    ethernet_register_protocol(ETH_TYPE_ARP, arp_handle_packet);
    ethernet_register_protocol(ETH_TYPE_IPV4, ip_receive);
    
    kinfo("Network stack initialized\n");
    return ERR_OK;
}

/**
 * Register a network device
 */
error_code_t network_register_device(net_device_t* device) {
    if (!device || !network_state.initialized) {
        return ERR_INVALID_ARG;
    }
    
    spinlock_lock(&network_state.lock);
    
    // Add to device list
    device->next = network_state.devices;
    network_state.devices = device;
    
    spinlock_unlock(&network_state.lock);
    
    kinfo("Registered network device: %s (MAC: %02x:%02x:%02x:%02x:%02x:%02x)\n",
          device->name,
          device->mac_address[0], device->mac_address[1], device->mac_address[2],
          device->mac_address[3], device->mac_address[4], device->mac_address[5]);
    
    return ERR_OK;
}

/**
 * Find network device by name
 */
net_device_t* network_find_device(const char* name) {
    if (!name || !network_state.initialized) {
        return NULL;
    }
    
    spinlock_lock(&network_state.lock);
    
    net_device_t* device = network_state.devices;
    while (device) {
        if (strcmp(device->name, name) == 0) {
            spinlock_unlock(&network_state.lock);
            return device;
        }
        device = device->next;
    }
    
    spinlock_unlock(&network_state.lock);
    return NULL;
}

/**
 * Set IP configuration for device
 */
error_code_t network_set_ip(net_device_t* device, uint32_t ip, uint32_t netmask, uint32_t gateway) {
    if (!device) {
        return ERR_INVALID_ARG;
    }
    
    device->ip_address = ip;
    device->netmask = netmask;
    device->gateway = gateway;
    
    kinfo("Network device %s: IP=%u.%u.%u.%u, Netmask=%u.%u.%u.%u, Gateway=%u.%u.%u.%u\n",
          device->name,
          (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
          (netmask >> 24) & 0xFF, (netmask >> 16) & 0xFF, (netmask >> 8) & 0xFF, netmask & 0xFF,
          (gateway >> 24) & 0xFF, (gateway >> 16) & 0xFF, (gateway >> 8) & 0xFF, gateway & 0xFF);
    
    return ERR_OK;
}

