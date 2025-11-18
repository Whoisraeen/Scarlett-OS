/**
 * @file stdio.c
 * @brief Standard I/O implementation (scanf variants)
 */

#include "../include/stdio.h"
#include "../include/string.h"
#include "../include/kprintf.h"
#include "../include/types.h"
#include <stdarg.h>

/**
 * Skip whitespace
 */
static void skip_whitespace(const char** str) {
    while (**str == ' ' || **str == '\t' || **str == '\n' || **str == '\r') {
        (*str)++;
    }
}

/**
 * Parse integer from string
 */
static long parse_int(const char** str, int base) {
    long value = 0;
    bool negative = false;
    
    skip_whitespace(str);
    
    if (**str == '-') {
        negative = true;
        (*str)++;
    } else if (**str == '+') {
        (*str)++;
    }
    
    while (**str) {
        char c = **str;
        int digit = -1;
        
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'z' && base > 10) {
            digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'Z' && base > 10) {
            digit = c - 'A' + 10;
        } else {
            break;
        }
        
        if (digit >= base) {
            break;
        }
        
        value = value * base + digit;
        (*str)++;
    }
    
    return negative ? -value : value;
}

/**
 * Parse unsigned integer from string
 */
static unsigned long parse_uint(const char** str, int base) {
    unsigned long value = 0;
    
    skip_whitespace(str);
    
    while (**str) {
        char c = **str;
        int digit = -1;
        
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'z' && base > 10) {
            digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'Z' && base > 10) {
            digit = c - 'A' + 10;
        } else {
            break;
        }
        
        if (digit >= base) {
            break;
        }
        
        value = value * base + digit;
        (*str)++;
    }
    
    return value;
}

/**
 * Parse floating point from string (simplified)
 */
static double parse_float(const char** str) {
    double value = 0.0;
    bool negative = false;
    
    skip_whitespace(str);
    
    if (**str == '-') {
        negative = true;
        (*str)++;
    } else if (**str == '+') {
        (*str)++;
    }
    
    // Parse integer part
    while (**str >= '0' && **str <= '9') {
        value = value * 10.0 + (**str - '0');
        (*str)++;
    }
    
    // Parse fractional part
    if (**str == '.') {
        (*str)++;
        double fraction = 0.1;
        while (**str >= '0' && **str <= '9') {
            value += (**str - '0') * fraction;
            fraction *= 0.1;
            (*str)++;
        }
    }
    
    // Parse exponent
    if (**str == 'e' || **str == 'E') {
        (*str)++;
        bool exp_negative = false;
        if (**str == '-') {
            exp_negative = true;
            (*str)++;
        } else if (**str == '+') {
            (*str)++;
        }
        
        int exp = 0;
        while (**str >= '0' && **str <= '9') {
            exp = exp * 10 + (**str - '0');
            (*str)++;
        }
        
        double exp_mult = 1.0;
        for (int i = 0; i < exp; i++) {
            exp_mult *= exp_negative ? 0.1 : 10.0;
        }
        value *= exp_mult;
    }
    
    return negative ? -value : value;
}

/**
 * sscanf - Parse formatted input from string
 */
int sscanf(const char* str, const char* format, ...) {
    if (!str || !format) {
        return 0;
    }
    
    va_list args;
    va_start(args, format);
    
    int count = 0;
    const char* s = str;
    
    while (*format) {
        if (*format == '%') {
            format++;
            
            // Skip width and precision (not implemented)
            while (*format >= '0' && *format <= '9') {
                format++;
            }
            
            switch (*format) {
                case 'd':
                case 'i': {
                    int* ptr = va_arg(args, int*);
                    *ptr = (int)parse_int(&s, 10);
                    count++;
                    break;
                }
                
                case 'u': {
                    unsigned int* ptr = va_arg(args, unsigned int*);
                    *ptr = (unsigned int)parse_uint(&s, 10);
                    count++;
                    break;
                }
                
                case 'x':
                case 'X': {
                    unsigned int* ptr = va_arg(args, unsigned int*);
                    *ptr = (unsigned int)parse_uint(&s, 16);
                    count++;
                    break;
                }
                
                case 'o': {
                    unsigned int* ptr = va_arg(args, unsigned int*);
                    *ptr = (unsigned int)parse_uint(&s, 8);
                    count++;
                    break;
                }
                
                case 'f':
                case 'g':
                case 'e': {
                    double* ptr = va_arg(args, double*);
                    *ptr = parse_float(&s);
                    count++;
                    break;
                }
                
                case 's': {
                    char* ptr = va_arg(args, char*);
                    skip_whitespace(&s);
                    while (*s && *s != ' ' && *s != '\t' && *s != '\n') {
                        *ptr++ = *s++;
                    }
                    *ptr = '\0';
                    count++;
                    break;
                }
                
                case 'c': {
                    char* ptr = va_arg(args, char*);
                    *ptr = *s++;
                    count++;
                    break;
                }
                
                case '%': {
                    if (*s == '%') {
                        s++;
                    }
                    break;
                }
            }
            format++;
        } else if (*format == *s) {
            format++;
            s++;
        } else {
            break;
        }
    }
    
    va_end(args);
    return count;
}

/**
 * scanf - Parse formatted input (reads from stdin)
 */
int scanf(const char* format, ...) {
    // For now, scanf is not fully implemented (needs stdin reading)
    // This is a placeholder
    (void)format;
    return 0;
}

/**
 * strtol - Convert string to long
 */
long strtol(const char* nptr, char** endptr, int base) {
    if (!nptr) {
        if (endptr) *endptr = NULL;
        return 0;
    }
    
    const char* s = nptr;
    long value = parse_int(&s, base);
    
    if (endptr) {
        *endptr = (char*)s;
    }
    
    return value;
}

/**
 * strtoul - Convert string to unsigned long
 */
unsigned long strtoul(const char* nptr, char** endptr, int base) {
    if (!nptr) {
        if (endptr) *endptr = NULL;
        return 0;
    }
    
    const char* s = nptr;
    unsigned long value = parse_uint(&s, base);
    
    if (endptr) {
        *endptr = (char*)s;
    }
    
    return value;
}

/**
 * strtod - Convert string to double
 */
double strtod(const char* nptr, char** endptr) {
    if (!nptr) {
        if (endptr) *endptr = NULL;
        return 0.0;
    }
    
    const char* s = nptr;
    double value = parse_float(&s);
    
    if (endptr) {
        *endptr = (char*)s;
    }
    
    return value;
}

