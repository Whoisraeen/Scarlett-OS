/**
 * @file udp.c
 * @brief UDP protocol implementation
 */

#include "../include/net/udp.h"
#include "../include/net/ip.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"

/**
 * Send UDP packet
 */
error_code_t udp_send(uint32_t dest_ip, uint16_t dest_port, uint16_t src_port, void* data, size_t len) {
    if (!data || len == 0) {
        return ERR_INVALID_ARG;
    }
    
    // Allocate UDP packet
    size_t packet_size = sizeof(udp_header_t) + len;
    udp_header_t* packet = (udp_header_t*)kmalloc(packet_size);
    if (!packet) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Build UDP header
    packet->src_port = __builtin_bswap16(src_port);
    packet->dest_port = __builtin_bswap16(dest_port);
    packet->length = __builtin_bswap16(packet_size);
    packet->checksum = 0;  // Optional for UDP
    
    // Copy data
    memcpy(packet->data, data, len);
    
    // Send via IP
    error_code_t err = ip_send(dest_ip, IP_PROTOCOL_UDP, packet, packet_size);
    
    kfree(packet);
    return err;
}

/**
 * Receive UDP packet
 * TODO: Fix to match new IP layer signature
 */
error_code_t udp_receive(void* buffer, size_t* len, uint32_t* src_ip, uint16_t* src_port, uint16_t* dest_port) {
    if (!buffer || !len || !src_ip || !src_port || !dest_port) {
        return ERR_INVALID_ARG;
    }

    // TODO: Implement proper UDP receive with new IP layer API
    (void)buffer;
    (void)len;
    (void)src_ip;
    (void)src_port;
    (void)dest_port;
    return ERR_NOT_SUPPORTED;
}

