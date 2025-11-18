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

// File access modes
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

// Seek origin
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

// FILE structure
typedef struct {
    int fd;              // File descriptor
    int error;           // Error indicator
    int eof;             // End-of-file indicator
} FILE;

// Standard streams
extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

// File I/O functions
FILE* fopen(const char* pathname, const char* mode);
int fclose(FILE* stream);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
void clearerr(FILE* stream);

// scanf format parsing
int sscanf(const char* str, const char* format, ...);
int scanf(const char* format, ...);

// String to number conversions
long strtol(const char* nptr, char** endptr, int base);
unsigned long strtoul(const char* nptr, char** endptr, int base);
double strtod(const char* nptr, char** endptr);

#endif // KERNEL_STDIO_H

