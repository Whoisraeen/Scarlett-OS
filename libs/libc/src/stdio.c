/**
 * @file stdio.c
 * @brief Standard I/O functions
 */

#include <stddef.h>
#include <stdint.h>
#include "syscall.h"

// Standard file descriptors
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/**
 * Write to file descriptor
 */
ssize_t write(int fd, const void* buf, size_t count) {
    return sys_write(fd, buf, count);
}

/**
 * Read from file descriptor
 */
ssize_t read(int fd, void* buf, size_t count) {
    return sys_read(fd, buf, count);
}

/**
 * Write string to stdout
 */
int puts(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    ssize_t written = write(STDOUT_FILENO, s, len);
    if (written < 0) return -1;
    write(STDOUT_FILENO, "\n", 1);
    return 0;
}

/**
 * Write character to stdout
 */
int putchar(int c) {
    char ch = (char)c;
    if (write(STDOUT_FILENO, &ch, 1) < 0) return -1;
    return c;
}

