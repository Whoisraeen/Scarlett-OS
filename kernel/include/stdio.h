/**
 * @file stdio.h
 * @brief Standard I/O functions
 */

#ifndef KERNEL_STDIO_H
#define KERNEL_STDIO_H

#include "types.h"

// Standard input/output/error file descriptors
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

// scanf format parsing
int sscanf(const char* str, const char* format, ...);
int scanf(const char* format, ...);

// String to number conversions
long strtol(const char* nptr, char** endptr, int base);
unsigned long strtoul(const char* nptr, char** endptr, int base);
double strtod(const char* nptr, char** endptr);

#endif // KERNEL_STDIO_H

