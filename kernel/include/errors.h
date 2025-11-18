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
    ERR_INVALID_STATE = -9,
    ERR_AGAIN = -10,
    ERR_FAILED = -11,
    
    // Memory errors
    ERR_INVALID_ADDRESS = -12,
    ERR_PAGE_FAULT = -13,
    ERR_OUT_OF_PAGES = -14,
    ERR_MAPPING_FAILED = -15,
    
    // Process errors
    ERR_INVALID_PID = -30,
    ERR_PROCESS_NOT_FOUND = -31,
    ERR_PROCESS_ALREADY_RUNNING = -32,
    ERR_CANNOT_CREATE_PROCESS = -33,
    
    // File system errors
    ERR_FILE_NOT_FOUND = -40,
    ERR_FILE_EXISTS = -41,
    ERR_DIRECTORY_NOT_FOUND = -42,
    ERR_NOT_A_DIRECTORY = -43,
    ERR_NOT_DIRECTORY = -44,
    ERR_IS_DIRECTORY = -45,
    ERR_END_OF_FILE = -46,
    ERR_READ_ONLY = -47,
    ERR_DISK_FULL = -48,
    
    // I/O errors
    ERR_IO_ERROR = -50,
    ERR_DEVICE_NOT_FOUND = -51,
    ERR_DEVICE_BUSY = -52,
    
    // System call errors
    ERR_INVALID_SYSCALL = -60,
    ERR_SYSCALL_FAILED = -61,
    
    // ELF errors
    ERR_INVALID_ELF = -70,
    ERR_ELF_NOT_EXECUTABLE = -71,
    ERR_ELF_LOAD_FAILED = -72,
    
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

