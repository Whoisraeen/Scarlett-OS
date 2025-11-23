/**
 * @file rtc.h
 * @brief Real Time Clock (RTC) driver interface
 */

#ifndef KERNEL_DRIVERS_RTC_H
#define KERNEL_DRIVERS_RTC_H

#include "../../types.h"

// Date and time structure
typedef struct {
    uint16_t year;      // Full year (e.g., 2023)
    uint8_t month;      // 1-12
    uint8_t day;        // 1-31
    uint8_t hour;       // 0-23
    uint8_t minute;     // 0-59
    uint8_t second;     // 0-59
} rtc_time_t;

/**
 * Initialize RTC driver
 */
void rtc_init(void);

/**
 * Get current wall-clock time
 * @param time Pointer to structure to fill
 */
void rtc_get_time(rtc_time_t* time);

#endif // KERNEL_DRIVERS_RTC_H
