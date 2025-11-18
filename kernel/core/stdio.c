/**
 * @file stdio.c
 * @brief Standard I/O implementation (scanf variants and File I/O)
 */

#include "../include/stdio.h"
#include "../include/string.h"
#include "../include/kprintf.h"
#include "../include/types.h"
#include "../include/fs/vfs.h"
#include "../include/mm/heap.h"
#include "../include/errors.h"
#include <stdarg.h>

// Maximum open files
#define MAX_OPEN_FILES 64

// FILE structure pool
static FILE file_pool[MAX_OPEN_FILES];
static bool file_pool_used[MAX_OPEN_FILES];

// Standard streams (initialized on first use)
static FILE stdin_file = {.fd = STDIN_FILENO, .error = 0, .eof = 0};
static FILE stdout_file = {.fd = STDOUT_FILENO, .error = 0, .eof = 0};
static FILE stderr_file = {.fd = STDERR_FILENO, .error = 0, .eof = 0};

FILE* stdin = &stdin_file;
FILE* stdout = &stdout_file;
FILE* stderr = &stderr_file;

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

/**
 * Convert mode string to VFS flags
 */
static uint64_t parse_mode(const char* mode) {
    uint64_t flags = 0;
    
    if (!mode || !mode[0]) {
        return 0;
    }
    
    // Parse mode string (r, w, a, r+, w+, a+)
    if (mode[0] == 'r') {
        flags |= VFS_MODE_READ;
        if (mode[1] == '+') {
            flags |= VFS_MODE_WRITE;
        }
    } else if (mode[0] == 'w') {
        flags |= VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC;
        if (mode[1] == '+') {
            flags |= VFS_MODE_READ;
        }
    } else if (mode[0] == 'a') {
        flags |= VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_APPEND;
        if (mode[1] == '+') {
            flags |= VFS_MODE_READ;
        }
    }
    
    return flags;
}

/**
 * Allocate FILE structure
 */
static FILE* alloc_file(void) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!file_pool_used[i]) {
            file_pool_used[i] = true;
            file_pool[i].fd = -1;
            file_pool[i].error = 0;
            file_pool[i].eof = 0;
            return &file_pool[i];
        }
    }
    return NULL;
}

/**
 * Free FILE structure
 */
static void free_file(FILE* file) {
    if (!file) {
        return;
    }
    
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (&file_pool[i] == file) {
            file_pool_used[i] = false;
            return;
        }
    }
}

/**
 * Open a file
 */
FILE* fopen(const char* pathname, const char* mode) {
    if (!pathname || !mode) {
        return NULL;
    }
    
    // Allocate FILE structure
    FILE* file = alloc_file();
    if (!file) {
        return NULL;
    }
    
    // Parse mode
    uint64_t flags = parse_mode(mode);
    if (flags == 0) {
        free_file(file);
        return NULL;
    }
    
    // Open file via VFS
    fd_t fd;
    error_code_t err = vfs_open(pathname, flags, &fd);
    if (err != ERR_OK) {
        free_file(file);
        return NULL;
    }
    
    file->fd = fd;
    file->error = 0;
    file->eof = 0;
    
    return file;
}

/**
 * Close a file
 */
int fclose(FILE* stream) {
    if (!stream) {
        return -1;
    }
    
    // Special handling for standard streams
    if (stream == stdin || stream == stdout || stream == stderr) {
        // Don't close standard streams
        return 0;
    }
    
    // Close file descriptor
    if (stream->fd >= 0) {
        vfs_close(stream->fd);
    }
    
    // Free FILE structure
    free_file(stream);
    
    return 0;
}

/**
 * Read from file
 */
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (!ptr || !stream || size == 0 || nmemb == 0) {
        return 0;
    }
    
    if (stream->error) {
        return 0;
    }
    
    size_t total_bytes = size * nmemb;
    size_t bytes_read = 0;
    
    error_code_t err = vfs_read(stream->fd, ptr, total_bytes, &bytes_read);
    if (err != ERR_OK) {
        stream->error = 1;
        return 0;
    }
    
    if (bytes_read < total_bytes) {
        stream->eof = 1;
    }
    
    // Return number of items read (not bytes)
    return bytes_read / size;
}

/**
 * Write to file
 */
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (!ptr || !stream || size == 0 || nmemb == 0) {
        return 0;
    }
    
    if (stream->error) {
        return 0;
    }
    
    size_t total_bytes = size * nmemb;
    size_t bytes_written = 0;
    
    error_code_t err = vfs_write(stream->fd, ptr, total_bytes, &bytes_written);
    if (err != ERR_OK) {
        stream->error = 1;
        return 0;
    }
    
    // Return number of items written (not bytes)
    return bytes_written / size;
}

/**
 * Seek in file
 */
int fseek(FILE* stream, long offset, int whence) {
    if (!stream) {
        return -1;
    }
    
    if (stream->error) {
        return -1;
    }
    
    error_code_t err = vfs_seek(stream->fd, offset, whence);
    if (err != ERR_OK) {
        stream->error = 1;
        return -1;
    }
    
    // Clear EOF flag on successful seek
    stream->eof = 0;
    
    return 0;
}

/**
 * Get current file position
 */
long ftell(FILE* stream) {
    if (!stream) {
        return -1;
    }
    
    if (stream->error) {
        return -1;
    }
    
    size_t position;
    error_code_t err = vfs_tell(stream->fd, &position);
    if (err != ERR_OK) {
        return -1;
    }
    
    return (long)position;
}

/**
 * Check end-of-file indicator
 */
int feof(FILE* stream) {
    if (!stream) {
        return 0;
    }
    return stream->eof;
}

/**
 * Check error indicator
 */
int ferror(FILE* stream) {
    if (!stream) {
        return 0;
    }
    return stream->error;
}

/**
 * Clear error and EOF indicators
 */
void clearerr(FILE* stream) {
    if (!stream) {
        return;
    }
    stream->error = 0;
    stream->eof = 0;
}

