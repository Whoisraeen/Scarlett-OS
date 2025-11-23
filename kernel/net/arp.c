/**
 * @file arp.c
 * @brief ARP protocol implementation
 */

#include "../include/net/arp.h"
#include "../include/net/ethernet.h"
#include "../include/net/network.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"
#include "../include/sync/spinlock.h"
#include "../include/time.h"

// ARP cache
#define ARP_CACHE_SIZE 64
#define ARP_CACHE_TIMEOUT 30000000000ULL  // 30 seconds in nanoseconds (simplified)

static struct {
    arp_cache_entry_t cache[ARP_CACHE_SIZE];
    spinlock_t lock;
    bool initialized;
} arp_state = {0};

// Forward declarations
extern error_code_t ethernet_send(void* data, size_t len, uint8_t* dest_mac, uint16_t type, net_device_t* device);
extern net_device_t* network_find_device(const char* name);

/**
 * Initialize ARP
 */
error_code_t arp_init(void) {
    if (arp_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing ARP...\n");
    
    memset(arp_state.cache, 0, sizeof(arp_state.cache));
    spinlock_init(&arp_state.lock);
    arp_state.initialized = true;
    
    kinfo("ARP initialized\n");
    return ERR_OK;
}

/**
 * Update ARP cache
 */
void arp_update_cache(uint32_t ip_address, uint8_t* mac_address) {
    if (!arp_state.initialized || !mac_address) {
        return;
    }
    
    spinlock_lock(&arp_state.lock);
    
    // Find existing entry or free slot
    arp_cache_entry_t* entry = NULL;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_state.cache[i].ip_address == ip_address) {
            entry = &arp_state.cache[i];
            break;
        }
        if (!entry && !arp_state.cache[i].valid) {
            entry = &arp_state.cache[i];
        }
    }
    
    if (!entry) {
        // Cache full, replace oldest (simple: replace first)
        entry = &arp_state.cache[0];
    }
    
    // Update entry
    entry->ip_address = ip_address;
    memcpy(entry->mac_address, mac_address, 6);
    entry->timestamp = time_get_uptime_ms();  // Use actual timestamp
    entry->valid = true;
    
    spinlock_unlock(&arp_state.lock);
}

/**
 * Resolve IP address to MAC address
 */
error_code_t arp_resolve(uint32_t ip_address, uint8_t* mac_address) {
    if (!arp_state.initialized || !mac_address) {
        return ERR_INVALID_ARG;
    }
    
    spinlock_lock(&arp_state.lock);
    
    // Search cache
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_state.cache[i].valid && arp_state.cache[i].ip_address == ip_address) {
            memcpy(mac_address, arp_state.cache[i].mac_address, 6);
            spinlock_unlock(&arp_state.lock);
            return ERR_OK;
        }
    }
    
    spinlock_unlock(&arp_state.lock);
    
    // Not in cache - send ARP request
    return arp_request(ip_address);
}

/**
 * Send ARP request
 */
error_code_t arp_request(uint32_t ip_address) {
    if (!arp_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    net_device_t* device = network_find_device("eth0");
    if (!device || !device->up) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    // Allocate ARP packet
    arp_packet_t* arp = (arp_packet_t*)kmalloc(sizeof(arp_packet_t));
    if (!arp) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Build ARP request
    arp->hardware_type = __builtin_bswap16(ARP_HW_TYPE_ETHERNET);
    arp->protocol_type = __builtin_bswap16(ARP_PROTO_TYPE_IPV4);
    arp->hardware_size = 6;
    arp->protocol_size = 4;
    arp->operation = __builtin_bswap16(ARP_OP_REQUEST);
    
    memcpy(arp->sender_mac, device->mac_address, 6);
    arp->sender_ip = device->ip_address;
    memset(arp->target_mac, 0, 6);  // Unknown
    arp->target_ip = ip_address;
    
    // Send ARP request (broadcast)
    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    error_code_t err = ethernet_send(arp, sizeof(arp_packet_t), broadcast_mac, ETH_TYPE_ARP, device);
    
    kfree(arp);
    return err;
}

/**
 * Handle incoming ARP packet
 */
error_code_t arp_handle_packet(void* buffer, size_t len) {
    if (!arp_state.initialized || !buffer || len < sizeof(arp_packet_t)) {
        return ERR_INVALID_ARG;
    }
    
    arp_packet_t* arp = (arp_packet_t*)buffer;
    
    // Check if it's for IPv4 over Ethernet
    if (__builtin_bswap16(arp->hardware_type) != ARP_HW_TYPE_ETHERNET ||
        __builtin_bswap16(arp->protocol_type) != ARP_PROTO_TYPE_IPV4) {
        return ERR_NOT_SUPPORTED;
    }
    
    net_device_t* device = network_find_device("eth0");
    if (!device || !device->up) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    uint16_t operation = __builtin_bswap16(arp->operation);
    
    if (operation == ARP_OP_REQUEST) {
        // ARP request - check if it's for us
        if (arp->target_ip == device->ip_address) {
            // Send ARP reply
            arp_packet_t reply = {0};
            reply.hardware_type = __builtin_bswap16(ARP_HW_TYPE_ETHERNET);
            reply.protocol_type = __builtin_bswap16(ARP_PROTO_TYPE_IPV4);
            reply.hardware_size = 6;
            reply.protocol_size = 4;
            reply.operation = __builtin_bswap16(ARP_OP_REPLY);
            
            memcpy(reply.sender_mac, device->mac_address, 6);
            reply.sender_ip = device->ip_address;
            memcpy(reply.target_mac, arp->sender_mac, 6);
            reply.target_ip = arp->sender_ip;
            
            // Send reply
            uint8_t dest_mac[6];
            memcpy(dest_mac, arp->sender_mac, 6);
            ethernet_send(&reply, sizeof(arp_packet_t), dest_mac, ETH_TYPE_ARP, device);
        }
    } else if (operation == ARP_OP_REPLY) {
        // ARP reply - update cache
        arp_update_cache(arp->sender_ip, arp->sender_mac);
    }
    
    return ERR_OK;
}

