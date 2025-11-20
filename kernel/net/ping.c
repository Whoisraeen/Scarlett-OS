/**
 * @file ping.c
 * @brief Ping utility implementation
 */

#include "../include/net/ping.h"
#include "../include/net/icmp.h"
#include "../include/net/dns.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

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
    
    // Send ICMP echo request
    error_code_t err = icmp_send_echo(dest_ip, 1, 1, NULL, 0);
    if (err != ERR_OK) {
        kerror("PING: Failed to send echo request\n");
        return err;
    }
    
    // TODO: Wait for echo reply and measure time
    // For now, return success with placeholder time
    *response_time_ms = 0;  // Placeholder
    
    kinfo("PING: Echo request sent (reply handling not implemented)\n");
    return ERR_OK;
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

