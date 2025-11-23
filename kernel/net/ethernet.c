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

// Protocol handlers
#define MAX_ETH_HANDLERS 16
static struct {
    uint16_t type;
    ethernet_protocol_handler_t handler;
} eth_handlers[MAX_ETH_HANDLERS];

static int eth_handler_count = 0;

/**
 * Initialize Ethernet subsystem
 */
error_code_t ethernet_init(void) {
    memset(eth_handlers, 0, sizeof(eth_handlers));
    eth_handler_count = 0;
    return ERR_OK;
}

/**
 * Register a protocol handler
 */
error_code_t ethernet_register_protocol(uint16_t type, ethernet_protocol_handler_t handler) {
    if (eth_handler_count >= MAX_ETH_HANDLERS) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Check if already registered
    for (int i = 0; i < eth_handler_count; i++) {
        if (eth_handlers[i].type == type) {
            eth_handlers[i].handler = handler; // Update
            return ERR_OK;
        }
    }
    
    eth_handlers[eth_handler_count].type = type;
    eth_handlers[eth_handler_count].handler = handler;
    eth_handler_count++;
    
    return ERR_OK;
}

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
    
    // Dispatch to protocol handler
    for (int i = 0; i < eth_handler_count; i++) {
        if (eth_handlers[i].type == eth_type) {
            return eth_handlers[i].handler(device, frame->data, payload_len);
        }
    }
    
    // No handler found
    // memmove(buffer, frame->data, payload_len); // Don't modify buffer if no handler
    // *len = payload_len;
    
    return ERR_NOT_SUPPORTED;
}

