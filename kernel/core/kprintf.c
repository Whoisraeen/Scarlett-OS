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
            
            // Handle flags
            bool left_align = false;
            bool zero_pad = false;
            if (*fmt == '-') {
                left_align = true;
                fmt++;
            }
            if (*fmt == '0') {
                zero_pad = true;
                fmt++;
            }
            
            // Handle width
            int width = 0;
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }

            // Handle long modifier (single or double)
            bool is_long = false;
            bool is_long_long = false;
            if (*fmt == 'l') {
                fmt++;
                if (*fmt == 'l') {
                    is_long_long = true;
                    fmt++;
                } else {
                    is_long = true;
                }
            }
            
            switch (*fmt) {
                case 's': {
                    // String
                    const char* str = va_arg(args, const char*);
                    if (!str) {
                        str = "(null)";
                    }
                    int len = strlen(str);
                    if (width > len && !left_align) {
                        // Right align: pad with spaces
                        for (int i = 0; i < width - len; i++) {
                            kputc(' ');
                            count++;
                        }
                    }
                    kputs(str);
                    count += len;
                    if (width > len && left_align) {
                        // Left align: pad with spaces after
                        for (int i = 0; i < width - len; i++) {
                            kputc(' ');
                            count++;
                        }
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
                    uint64_t val;
                    if (is_long_long) {
                        val = va_arg(args, uint64_t);
                    } else if (is_long) {
                        val = va_arg(args, uint64_t);
                    } else {
                        val = va_arg(args, unsigned int);
                    }
                    uitoa(val, buf, 10);
                    int len = strlen(buf);
                    if (width > len && !left_align) {
                        // Pad with zeros or spaces
                        char pad_char = zero_pad ? '0' : ' ';
                        for (int i = 0; i < width - len; i++) {
                            kputc(pad_char);
                            count++;
                        }
                    }
                    kputs(buf);
                    count += len;
                    if (width > len && left_align) {
                        // Left align: pad with spaces after
                        for (int i = 0; i < width - len; i++) {
                            kputc(' ');
                            count++;
                        }
                    }
                    break;
                }
                
                case 'x': {
                    // Hexadecimal
                    uint64_t val;
                    if (is_long_long) {
                        val = va_arg(args, uint64_t);
                    } else if (is_long) {
                        val = va_arg(args, uint64_t);
                    } else {
                        val = va_arg(args, unsigned int);
                    }
                    uitoa(val, buf, 16);
                    int len = strlen(buf);
                    if (width > len && !left_align) {
                        // Pad with zeros or spaces
                        char pad_char = zero_pad ? '0' : ' ';
                        for (int i = 0; i < width - len; i++) {
                            kputc(pad_char);
                            count++;
                        }
                    }
                    kputs(buf);
                    count += len;
                    if (width > len && left_align) {
                        // Left align: pad with spaces after
                        for (int i = 0; i < width - len; i++) {
                            kputc(' ');
                            count++;
                        }
                    }
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
                    if (left_align) kputc('-');
                    if (zero_pad) kputc('0');
                    if (width > 0) {
                        char width_buf[16];
                        uitoa(width, width_buf, 10);
                        kputs(width_buf);
                        count += strlen(width_buf);
                    }
                    if (is_long_long) {
                        kputs("ll");
                        count += 2;
                    } else if (is_long) {
                        kputc('l');
                        count++;
                    }
                    kputc(*fmt);
                    count += 1 + (left_align ? 1 : 0) + (zero_pad ? 1 : 0);
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