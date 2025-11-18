/**
 * @file vga.h
 * @brief VGA text mode interface
 */

#ifndef KERNEL_VGA_H
#define KERNEL_VGA_H

#include "types.h"

/**
 * Initialize VGA text mode
 */
void vga_init(void);

/**
 * Set text color
 */
void vga_setcolor(uint8_t color);

/**
 * Write a character to screen
 */
void vga_putchar(char c);

/**
 * Write a string to screen
 */
void vga_writestring(const char* data);

/**
 * Write data to screen
 */
void vga_write(const char* data, size_t size);

#endif // KERNEL_VGA_H

