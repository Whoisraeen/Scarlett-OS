/**
 * @file kprintf.c
 * @brief Kernel printf implementation
 * 
 * A simple printf implementation for kernel debugging.
 * Supports basic format specifiers: %s, %c, %d, %u, %x, %p, %lu, %lx
 */

#include "../include/kprintf.h"
#include "../include/types.h"
#include "../include/string.h"

// External serial functions
extern void serial_putc(char c);
extern void serial_puts(const char* str);

/**
 * Print a single character
 */
void kputc(char c) {
    serial_putc(c);
}

/**
 * Print a string
 */
void kputs(const char* str) {
    serial_puts(str);
}

/**
 * Convert unsigned integer to string
 * Fixed: Added bounds checking to prevent buffer overflow
 */
static void uitoa(uint64_t value, char* buf, int base) {
    const char* digits = "0123456789ABCDEF";
    char temp[32];
    int i = 0;
    
    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    
    // Prevent buffer overflow
    while (value > 0 && i < 31) {
        temp[i++] = digits[value % base];
        value /= base;
    }
    
    int j = 0;
    while (i > 0 && j < 31) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

/**
 * Convert signed integer to string
 */
static void itoa(int64_t value, char* buf, int base) {
    if (value < 0 && base == 10) {
        buf[0] = '-';
        uitoa(-value, buf + 1, base);
    } else {
        uitoa(value, buf, base);
    }
}



/**
 * Print formatted string with va_list
 */
int kvprintf(const char* fmt, va_list args) {
    int count = 0;
    char buf[32];
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            
            // Handle width and padding
            int width = 0;
            bool zero_pad = false;
            if (*fmt == '0') {
                zero_pad = true;
                fmt++;
            }
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }

            // Handle long modifier
            bool is_long = false;
            if (*fmt == 'l') {
                is_long = true;
                fmt++;
            }
            
            switch (*fmt) {
                case 's': {
                    // String
                    const char* str = va_arg(args, const char*);
                    if (str) {
                        kputs(str);
                        count += strlen(str);
                    } else {
                        kputs("(null)");
                        count += 6;
                    }
                    break;
                }
                
                case 'c': {
                    // Character
                    char c = (char)va_arg(args, int);
                    kputc(c);
                    count++;
                    break;
                }
                
                case 'd': {
                    // Signed decimal
                    int64_t val = is_long ? va_arg(args, int64_t) : va_arg(args, int);
                    itoa(val, buf, 10);
                    kputs(buf);
                    count += strlen(buf);
                    break;
                }
                
                case 'u': {
                    // Unsigned decimal
                    uint64_t val = is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                    uitoa(val, buf, 10);
                    kputs(buf);
                    count += strlen(buf);
                    break;
                }
                
                case 'x': {
                    // Hexadecimal
                    uint64_t val = is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                    uitoa(val, buf, 16);
                    int len = strlen(buf);
                    if (width > len && zero_pad) {
                        for (int i = 0; i < width - len; i++) {
                            kputc('0');
                            count++;
                        }
                    }
                    kputs(buf);
                    count += strlen(buf);
                    break;
                }
                
                case 'p': {
                    // Pointer
                    uint64_t val = (uint64_t)va_arg(args, void*);
                    kputs("0x");
                    uitoa(val, buf, 16);
                    // Pad with zeros to 16 characters
                    int len = strlen(buf);
                    for (int i = 0; i < 16 - len; i++) {
                        kputc('0');
                        count++;
                    }
                    kputs(buf);
                    count += 2 + strlen(buf);
                    break;
                }
                
                case '%': {
                    // Literal %
                    kputc('%');
                    count++;
                    break;
                }
                
                default:
                    // Unknown format, print as-is
                    kputc('%');
                    if (zero_pad) kputc('0');
                    // This part is tricky, would need to print the width digits too
                    if (is_long) kputc('l');
                    kputc(*fmt);
                    count += 2 + (is_long ? 1 : 0) + (zero_pad ? 1 : 0);
                    break;
            }
        } else {
            kputc(*fmt);
            count++;
        }
        fmt++;
    }
    
    return count;
}

/**
 * Print formatted string
 */
int kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int count = kvprintf(fmt, args);
    va_end(args);
    return count;
}

