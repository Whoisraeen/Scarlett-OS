/**
 * @file kprintf.h
 * @brief Kernel printf functionality
 */

#ifndef KERNEL_KPRINTF_H
#define KERNEL_KPRINTF_H

#include <stdarg.h>

/**
 * Print formatted string to kernel console (serial)
 */
int kprintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * Print formatted string with va_list
 */
int kvprintf(const char* fmt, va_list args);

/**
 * Print a string
 */
void kputs(const char* str);

/**
 * Print a single character
 */
void kputc(char c);

#endif // KERNEL_KPRINTF_H

