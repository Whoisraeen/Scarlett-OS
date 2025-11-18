/**
 * @file debug.h
 * @brief Debugging macros and functions
 */

#ifndef KERNEL_DEBUG_H
#define KERNEL_DEBUG_H

#include "kprintf.h"

/**
 * Panic - print error message and halt
 */
void kpanic(const char* msg) __attribute__((noreturn));

/**
 * Assert macro
 */
#define kassert(cond, msg) \
    do { \
        if (!(cond)) { \
            kprintf("[ASSERT FAILED] %s:%d: %s\n", __FILE__, __LINE__, msg); \
            kpanic("Assertion failed"); \
        } \
    } while (0)

/**
 * Debug print macro (can be disabled in release builds)
 */
#ifdef DEBUG_BUILD
#define kdebug(fmt, ...) kprintf("[DEBUG] " fmt, ##__VA_ARGS__)
#else
#define kdebug(fmt, ...) ((void)0)
#endif

/**
 * Info print
 */
#define kinfo(fmt, ...) kprintf("[INFO] " fmt, ##__VA_ARGS__)

/**
 * Warning print
 */
#define kwarn(fmt, ...) kprintf("[WARN] " fmt, ##__VA_ARGS__)

/**
 * Error print
 */
#define kerror(fmt, ...) kprintf("[ERROR] " fmt, ##__VA_ARGS__)

#endif // KERNEL_DEBUG_H

