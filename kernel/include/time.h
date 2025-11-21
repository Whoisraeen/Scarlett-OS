/**
 * @file time.h
 * @brief Time functions
 */

#ifndef KERNEL_TIME_H
#define KERNEL_TIME_H

#include "types.h"

/**
 * Get system uptime in milliseconds
 */
uint64_t time_get_uptime_ms(void);

#endif // KERNEL_TIME_H

