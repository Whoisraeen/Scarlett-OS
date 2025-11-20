/**
 * @file stdio.h
 * @brief Standard I/O declarations
 */

#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

typedef long ssize_t;

ssize_t write(int fd, const void* buf, size_t count);
ssize_t read(int fd, void* buf, size_t count);
int puts(const char* s);
int putchar(int c);

#endif // STDIO_H

