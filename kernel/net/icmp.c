/**
 * @file icmp.c
 * @brief ICMP protocol implementation
 */

#include "../include/net/icmp.h"
#include "../include/net/ip.h"
#include "../include/net/network.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"

// Forward declarations
extern error_code_t ip_send(uint32_t dest_ip, uint8_t protocol, void* data, size_t len);
extern net_device_t* network_find_device(const char* name);

/**
 * Calculate ICMP checksum
 */
static uint16_t icmp_checksum(icmp_header_t* header, size_t len) {
    uint32_t sum = 0;
    uint16_t* words = (uint16_t*)header;
    size_t word_count = len / 2;
    
    for (size_t i = 0; i < word_count; i++) {
        sum += __builtin_bswap16(words[i]);
    }
    
    // Add padding byte if odd length
    if (len % 2) {
        sum += ((uint8_t*)header)[len - 1] << 8;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return __builtin_bswap16(~sum);
}

/**
 * Initialize ICMP
 */
error_code_t icmp_init(void) {
    kinfo("ICMP initialized\n");
    return ERR_OK;
}

/**
 * Send ICMP echo request (ping)
 */
error_code_t icmp_send_echo(uint32_t dest_ip, uint16_t identifier, uint16_t sequence, void* data, size_t len) {
    if (!data && len > 0) {
        return ERR_INVALID_ARG;
    }
    
    // Allocate ICMP packet
    size_t packet_size = sizeof(icmp_header_t) + len;
    icmp_header_t* icmp = (icmp_header_t*)kmalloc(packet_size);
    if (!icmp) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Build ICMP echo request
    icmp->type = ICMP_TYPE_ECHO_REQUEST;
    icmp->code = ICMP_CODE_ECHO;
    icmp->checksum = 0;
    icmp->data.echo.identifier = __builtin_bswap16(identifier);
    icmp->data.echo.sequence = __builtin_bswap16(sequence);
    
    // Copy payload
    if (data && len > 0) {
        memcpy(icmp->payload, data, len);
    }
    
    // Calculate checksum
    icmp->checksum = icmp_checksum(icmp, packet_size);
    
    // Send via IP
    error_code_t err = ip_send(dest_ip, IP_PROTOCOL_ICMP, icmp, packet_size);
    
    kfree(icmp);
    return err;
}

/**
 * Handle incoming ICMP packet
 */
error_code_t icmp_handle_packet(void* buffer, size_t len, uint32_t src_ip) {
    if (!buffer || len < sizeof(icmp_header_t)) {
        return ERR_INVALID_ARG;
    }
    
    icmp_header_t* icmp = (icmp_header_t*)buffer;
    
    // Verify checksum
    uint16_t checksum = icmp->checksum;
    icmp->checksum = 0;
    uint16_t calculated = icmp_checksum(icmp, len);
    icmp->checksum = checksum;
    
    if (checksum != calculated) {
        return ERR_FAILED;  // Invalid checksum
    }
    
    net_device_t* device = network_find_device("eth0");
    if (!device || !device->up) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    switch (icmp->type) {
        case ICMP_TYPE_ECHO_REQUEST: {
            // Echo request - send echo reply
            icmp_header_t reply = {0};
            reply.type = ICMP_TYPE_ECHO_REPLY;
            reply.code = ICMP_CODE_ECHO;
            reply.checksum = 0;
            reply.data.echo.identifier = icmp->data.echo.identifier;
            reply.data.echo.sequence = icmp->data.echo.sequence;
            
            // Copy payload
            size_t payload_len = len - sizeof(icmp_header_t);
            size_t reply_size = sizeof(icmp_header_t) + payload_len;
            icmp_header_t* reply_packet = (icmp_header_t*)kmalloc(reply_size);
            if (!reply_packet) {
                return ERR_OUT_OF_MEMORY;
            }
            
            memcpy(reply_packet, &reply, sizeof(icmp_header_t));
            if (payload_len > 0) {
                memcpy(reply_packet->payload, icmp->payload, payload_len);
            }
            
            // Calculate checksum
            reply_packet->checksum = icmp_checksum(reply_packet, reply_size);
            
            // Send reply
            error_code_t err = ip_send(src_ip, IP_PROTOCOL_ICMP, reply_packet, reply_size);
            kfree(reply_packet);
            return err;
        }
        
        case ICMP_TYPE_ECHO_REPLY: {
            // Echo reply - would notify ping application
            // For now, just acknowledge receipt
            return ERR_OK;
        }
        
        default:
            // Other ICMP messages - not handled yet
            return ERR_NOT_SUPPORTED;
    }
}

