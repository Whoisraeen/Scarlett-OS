/**
 * @file string.h
 * @brief String function declarations
 */

#ifndef STRING_H
#define STRING_H

#include <stddef.h>

char* strcpy(char* dest, const char* src);
void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
size_t strlen(const char* s);

#endif // STRING_H

