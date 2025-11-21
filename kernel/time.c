/**
 * @file time.c
 * @brief Time functions implementation
 */

#include "include/types.h"
#include "include/time.h"
#include "include/hal/timer.h"

/**
 * Get system uptime in milliseconds
 */
uint64_t time_get_uptime_ms(void) {
    return timer_get_ms();
}

