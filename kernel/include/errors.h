/**
 * @file errors.h
 * @brief Error code definitions
 * 
 * Centralized error code system for the kernel.
 */

#ifndef KERNEL_ERRORS_H
#define KERNEL_ERRORS_H

// Error codes (negative values indicate errors)
typedef enum {
    // Success
    ERR_OK = 0,
    
    // General errors
    ERR_INVALID_ARG = -1,
    ERR_OUT_OF_MEMORY = -2,
    ERR_NOT_FOUND = -3,
    ERR_ALREADY_EXISTS = -4,
    ERR_NOT_EMPTY = -39,
    ERR_PERMISSION_DENIED = -5,
    ERR_NOT_SUPPORTED = -6,
    ERR_TIMEOUT = -7,
    ERR_INTERRUPTED = -8,
    
    // Memory errors
    ERR_INVALID_ADDRESS = -10,
    ERR_PAGE_FAULT = -11,
    ERR_OUT_OF_PAGES = -12,
    ERR_MAPPING_FAILED = -13,
    
    // Process errors
    ERR_INVALID_PID = -20,
    ERR_PROCESS_NOT_FOUND = -21,
    ERR_PROCESS_ALREADY_RUNNING = -22,
    ERR_CANNOT_CREATE_PROCESS = -23,
    
    // File system errors
    ERR_FILE_NOT_FOUND = -30,
    ERR_FILE_EXISTS = -31,
    ERR_DIRECTORY_NOT_FOUND = -32,
    ERR_NOT_A_DIRECTORY = -33,
    ERR_NOT_DIRECTORY = -36,
    ERR_IS_DIRECTORY = -37,
    ERR_END_OF_FILE = -38,
    ERR_READ_ONLY = -34,
    ERR_DISK_FULL = -35,
    
    // I/O errors
    ERR_IO_ERROR = -40,
    ERR_DEVICE_NOT_FOUND = -41,
    ERR_DEVICE_BUSY = -42,
    
    // System call errors
    ERR_INVALID_SYSCALL = -50,
    ERR_SYSCALL_FAILED = -51,
    
    // ELF errors
    ERR_INVALID_ELF = -60,
    ERR_ELF_NOT_EXECUTABLE = -61,
    ERR_ELF_LOAD_FAILED = -62,
    
    // Unknown error
    ERR_UNKNOWN = -999
} error_code_t;

// Convert error code to string
const char* error_to_string(error_code_t err);

// Check if error code indicates success
static inline bool is_error(error_code_t err) {
    return err < 0;
}

// Check if error code indicates success
static inline bool is_success(error_code_t err) {
    return err == ERR_OK;
}

#endif // KERNEL_ERRORS_H

