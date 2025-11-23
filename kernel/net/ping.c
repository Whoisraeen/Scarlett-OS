/**
 * @file ping.c
 * @brief Ping utility implementation
 */

#include "../include/net/ping.h"
#include "../include/net/icmp.h"
#include "../include/net/dns.h"
#include "../include/net/socket.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/time.h"
#include "../include/mm/heap.h"
#include "../include/string.h"

/**
 * Send ping to IP address
 */
error_code_t ping_send(uint32_t dest_ip, uint8_t* response_time_ms) {
    if (!response_time_ms) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("PING: Sending ping to %u.%u.%u.%u\n",
          (dest_ip >> 24) & 0xFF,
          (dest_ip >> 16) & 0xFF,
          (dest_ip >> 8) & 0xFF,
          dest_ip & 0xFF);
    
    // Generate unique identifier and sequence for this ping
    static uint16_t ping_identifier = 1;
    static uint16_t ping_sequence = 1;
    uint16_t identifier = ping_identifier++;
    uint16_t sequence = ping_sequence++;
    
    // Record start time
    uint64_t start_time = time_get_uptime_ms();
    
    // Send ICMP echo request
    error_code_t err = icmp_send_echo(dest_ip, identifier, sequence, NULL, 0);
    if (err != ERR_OK) {
        kerror("PING: Failed to send echo request\n");
        return err;
    }
    
    // Wait for echo reply and measure time
    // Create raw socket to receive ICMP packets
    int sockfd = socket_create(2, SOCK_RAW, 1);  // AF_INET, SOCK_RAW, IPPROTO_ICMP
    if (sockfd < 0) {
        kerror("PING: Failed to create raw socket\n");
        *response_time_ms = 0;
        return ERR_FAILED;
    }
    
    // Set socket timeout (5 seconds)
    uint32_t timeout = 5000;  // 5 seconds in milliseconds
    socket_setsockopt(sockfd, 0, 0, &timeout, sizeof(timeout));  // Simplified timeout
    
    // Wait for reply with timeout
    uint8_t* buffer = (uint8_t*)kmalloc(1024);
    if (!buffer) {
        socket_close(sockfd);
        *response_time_ms = 0;
        return ERR_OUT_OF_MEMORY;
    }
    
    bool reply_received = false;
    uint64_t end_time = start_time;
    uint64_t timeout_time = start_time + timeout;
    
    // Poll for reply (simplified - would use proper socket receive)
    while (time_get_uptime_ms() < timeout_time) {
        size_t recv_len = 1024;
        error_code_t recv_err = socket_recv(sockfd, buffer, &recv_len, 0);
        
        if (recv_err == ERR_OK && recv_len >= sizeof(icmp_header_t)) {
            // Check if this is our echo reply
            icmp_header_t* icmp = (icmp_header_t*)(buffer + 20);  // Skip IP header (20 bytes)
            if (icmp->type == ICMP_TYPE_ECHO_REPLY &&
                __builtin_bswap16(icmp->data.echo.identifier) == identifier &&
                __builtin_bswap16(icmp->data.echo.sequence) == sequence) {
                end_time = time_get_uptime_ms();
                reply_received = true;
                break;
            }
        }
        
        // Small delay to avoid busy waiting
        for (volatile uint32_t delay = 0; delay < 10000; delay++);
    }
    
    kfree(buffer);
    socket_close(sockfd);
    
    if (reply_received) {
        *response_time_ms = (uint8_t)(end_time - start_time);
        kinfo("PING: Reply received in %u ms\n", *response_time_ms);
        return ERR_OK;
    } else {
        kerror("PING: No reply received (timeout)\n");
        *response_time_ms = 0;
        return ERR_TIMEOUT;
    }
}

/**
 * Send ping to hostname
 */
error_code_t ping_hostname(const char* hostname, uint8_t* response_time_ms) {
    if (!hostname || !response_time_ms) {
        return ERR_INVALID_ARG;
    }
    
    // Resolve hostname
    uint32_t ip_address;
    error_code_t err = dns_resolve(hostname, &ip_address);
    if (err != ERR_OK) {
        kerror("PING: Failed to resolve hostname %s\n", hostname);
        return err;
    }
    
    // Send ping
    return ping_send(ip_address, response_time_ms);
}

