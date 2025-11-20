/**
 * @file stdlib.h
 * @brief Standard library functions
 */

#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t num, size_t size);

void exit(int status);
void abort(void);

#endif // STDLIB_H

