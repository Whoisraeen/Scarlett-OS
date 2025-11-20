/**
 * @file ping.h
 * @brief Ping utility interface
 */

#ifndef KERNEL_NET_PING_H
#define KERNEL_NET_PING_H

#include "../types.h"
#include "../errors.h"

// Ping functions
error_code_t ping_send(uint32_t dest_ip, uint8_t* response_time_ms);
error_code_t ping_hostname(const char* hostname, uint8_t* response_time_ms);

#endif // KERNEL_NET_PING_H

