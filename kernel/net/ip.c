/**
 * @file ip.c
 * @brief IP protocol implementation
 */

#include "../include/net/ip.h"
#include "../include/net/ethernet.h"
#include "../include/net/network.h"
#include "../include/net/arp.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"

/**
 * Calculate IP checksum
 */
uint16_t ip_checksum(ip_header_t* header) {
    uint32_t sum = 0;
    uint16_t* words = (uint16_t*)header;
    size_t header_len = (header->version_ihl & 0x0F) * 4;
    
    for (size_t i = 0; i < header_len / 2; i++) {
        sum += __builtin_bswap16(words[i]);
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return __builtin_bswap16(~sum);
}

/**
 * Send IP packet
 */
error_code_t ip_send(uint32_t dest_ip, uint8_t protocol, void* data, size_t len) {
    if (!data || len == 0) {
        return ERR_INVALID_ARG;
    }
    
    // Find a network device (use first available)
    net_device_t* device = network_find_device("eth0");
    if (!device || !device->up) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    // Allocate IP packet
    size_t packet_size = sizeof(ip_header_t) + len;
    ip_header_t* packet = (ip_header_t*)kmalloc(packet_size);
    if (!packet) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Build IP header
    packet->version_ihl = 0x45;  // IPv4, 5 words (20 bytes)
    packet->tos = 0;
    packet->total_length = __builtin_bswap16(packet_size);
    packet->identification = 0;
    packet->flags_fragment = 0;
    packet->ttl = 64;
    packet->protocol = protocol;
    packet->checksum = 0;
    packet->src_ip = device->ip_address;
    packet->dest_ip = dest_ip;
    
    // Copy data
    memcpy(packet->data, data, len);
    
    // Calculate checksum
    packet->checksum = ip_checksum(packet);
    
    // Determine destination MAC
    uint8_t dest_mac[6];
    uint32_t next_hop_ip;
    
    // Check if destination is broadcast
    if (dest_ip == 0xFFFFFFFF) {
        memset(dest_mac, 0xFF, 6);
    } else {
        // Check if destination is on same network
        uint32_t src_net = device->ip_address & device->netmask;
        uint32_t dest_net = dest_ip & device->netmask;
        
        if (src_net == dest_net) {
            // Same network
            next_hop_ip = dest_ip;
        } else {
            // Different network - send to gateway
            next_hop_ip = device->gateway;
        }
        
        // Resolve MAC address via ARP
        error_code_t err = arp_resolve(next_hop_ip, dest_mac);
        if (err != ERR_OK) {
            kfree(packet);
            kerror("IP: ARP resolution failed for %08x\n", next_hop_ip);
            return err;
        }
    }
    
    // Send via Ethernet
    error_code_t err = ethernet_send(packet, packet_size, dest_mac, ETH_TYPE_IPV4, device);
    
    kfree(packet);
    return err;
}

/**
 * Receive IP packet
 */
error_code_t ip_receive(net_device_t* device, void* buffer, size_t len) {
    if (!device || !buffer) {
        return ERR_INVALID_ARG;
    }
    
    // Cast buffer to IP header (Ethernet header already stripped or we point to payload)
    // ethernet.c passes pointer to payload
    ip_header_t* ip_header = (ip_header_t*)buffer;
    
    // Verify checksum
    uint16_t checksum = ip_header->checksum;
    ip_header->checksum = 0;
    uint16_t calculated = ip_checksum(ip_header);
    ip_header->checksum = checksum;
    
    if (checksum != calculated) {
        return ERR_FAILED;  // Invalid checksum
    }
    
    // Check if packet is for us
    if (ip_header->dest_ip != device->ip_address) {
        // Check for broadcast
        if (ip_header->dest_ip != 0xFFFFFFFF) {
            return ERR_NOT_FOUND;  // Not for us
        }
    }
    
    // Extract payload
    size_t header_len = (ip_header->version_ihl & 0x0F) * 4;
    size_t payload_len = __builtin_bswap16(ip_header->total_length) - header_len;
    
    uint8_t proto = ip_header->protocol;
    uint32_t src = ip_header->src_ip;
    
    void* payload = (uint8_t*)buffer + header_len;
    
    // Handle ICMP packets
    if (proto == IP_PROTOCOL_ICMP) {
        extern error_code_t icmp_handle_packet(void* buffer, size_t len, uint32_t src_ip);
        icmp_handle_packet(payload, payload_len, src);
    }
    
    return ERR_OK;
}

