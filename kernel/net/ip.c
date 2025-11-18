/**
 * @file ip.c
 * @brief IP protocol implementation
 */

#include "../include/net/ip.h"
#include "../include/net/ethernet.h"
#include "../include/net/network.h"
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
    
    // Determine destination MAC (for now, use ARP or broadcast)
    // This is simplified - real implementation would use ARP
    uint8_t dest_mac[6];
    
    // Check if destination is on same network
    uint32_t src_net = device->ip_address & device->netmask;
    uint32_t dest_net = dest_ip & device->netmask;
    
    if (src_net == dest_net) {
        // Same network - need ARP (for now, use broadcast)
        memset(dest_mac, 0xFF, 6);
    } else {
        // Different network - send to gateway (for now, use broadcast)
        memset(dest_mac, 0xFF, 6);
    }
    
    // Send via Ethernet
    error_code_t err = ethernet_send(packet, packet_size, dest_mac, ETH_TYPE_IPV4, device);
    
    kfree(packet);
    return err;
}

/**
 * Receive IP packet
 */
error_code_t ip_receive(void* buffer, size_t* len, uint32_t* src_ip, uint8_t* protocol) {
    if (!buffer || !len || !src_ip || !protocol) {
        return ERR_INVALID_ARG;
    }
    
    // Find a network device
    net_device_t* device = network_find_device("eth0");
    if (!device || !device->up) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    // Receive Ethernet frame
    size_t frame_len = *len;
    error_code_t err = ethernet_receive(device, buffer, &frame_len);
    if (err != ERR_OK) {
        return err;
    }
    
    // Check if it's an IP packet
    ethernet_frame_t* eth_frame = (ethernet_frame_t*)buffer;
    if (__builtin_bswap16(eth_frame->type) != ETH_TYPE_IPV4) {
        return ERR_NOT_SUPPORTED;
    }
    
    // Extract IP header
    ip_header_t* ip_header = (ip_header_t*)eth_frame->data;
    
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
    
    memmove(buffer, ip_header->data, payload_len);
    *len = payload_len;
    *src_ip = src;
    *protocol = proto;
    
    // Handle ICMP packets
    if (proto == IP_PROTOCOL_ICMP) {
        extern error_code_t icmp_handle_packet(void* buffer, size_t len, uint32_t src_ip);
        icmp_handle_packet(buffer, payload_len, src);
    }
    
    return ERR_OK;
}

