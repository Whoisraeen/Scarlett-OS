/**
 * @file ethernet.c
 * @brief Ethernet protocol implementation
 */

#include "../include/net/ethernet.h"
#include "../include/net/network.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"

// Forward declaration
extern net_device_t* network_find_device(const char* name);

/**
 * Send Ethernet frame
 */
error_code_t ethernet_send(void* data, size_t len, uint8_t* dest_mac, uint16_t type, net_device_t* device) {
    if (!data || !dest_mac || !device || len == 0) {
        return ERR_INVALID_ARG;
    }
    
    // Allocate frame buffer
    size_t frame_size = ETH_HEADER_SIZE + len;
    if (frame_size < ETH_MIN_SIZE) {
        frame_size = ETH_MIN_SIZE;
    }
    
    ethernet_frame_t* frame = (ethernet_frame_t*)kmalloc(frame_size);
    if (!frame) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Build Ethernet header
    memcpy(frame->dest_mac, dest_mac, 6);
    memcpy(frame->src_mac, device->mac_address, 6);
    frame->type = __builtin_bswap16(type);  // Network byte order
    
    // Copy data
    memcpy(frame->data, data, len);
    
    // Pad if necessary
    if (frame_size > ETH_HEADER_SIZE + len) {
        memset(frame->data + len, 0, frame_size - ETH_HEADER_SIZE - len);
    }
    
    // Send via device driver
    error_code_t err = device->send_packet(device, frame, frame_size);
    
    kfree(frame);
    return err;
}

/**
 * Receive Ethernet frame
 */
error_code_t ethernet_receive(net_device_t* device, void* buffer, size_t* len) {
    if (!device || !buffer || !len) {
        return ERR_INVALID_ARG;
    }
    
    // Receive from device
    error_code_t err = device->receive_packet(device, buffer, len);
    if (err != ERR_OK) {
        return err;
    }
    
    if (*len < ETH_HEADER_SIZE) {
        return ERR_INVALID_ARG;
    }
    
    ethernet_frame_t* frame = (ethernet_frame_t*)buffer;
    
    // Check if frame is for us (or broadcast)
    bool is_broadcast = true;
    bool is_for_us = true;
    
    for (int i = 0; i < 6; i++) {
        if (frame->dest_mac[i] != 0xFF) {
            is_broadcast = false;
        }
        if (frame->dest_mac[i] != device->mac_address[i]) {
            is_for_us = false;
        }
    }
    
    if (!is_broadcast && !is_for_us) {
        return ERR_NOT_FOUND;  // Frame not for us
    }
    
    // Extract payload
    size_t payload_len = *len - ETH_HEADER_SIZE;
    uint16_t eth_type = __builtin_bswap16(frame->type);
    
    // Handle ARP packets
    if (eth_type == ETH_TYPE_ARP) {
        extern error_code_t arp_handle_packet(void* buffer, size_t len);
        arp_handle_packet(frame->data, payload_len);
        return ERR_NOT_FOUND;  // ARP handled, don't pass to IP layer
    }
    
    memmove(buffer, frame->data, payload_len);
    *len = payload_len;
    
    return ERR_OK;
}

