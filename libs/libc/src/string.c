/**
 * @file string.c
 * @brief Standard C string functions for user-space
 */

#include <stddef.h>
#include <stdint.h>

/**
 * Copy string
 */
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

/**
 * Copy n bytes
 */
void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

/**
 * Set memory
 */
void* memset(void* s, int c, size_t n) {
    uint8_t* p = (uint8_t*)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    return s;
}

/**
 * String length
 */
size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

