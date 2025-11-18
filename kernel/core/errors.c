/**
 * @file errors.c
 * @brief Error code implementation
 */

#include "../include/errors.h"

/**
 * Convert error code to string
 */
const char* error_to_string(error_code_t err) {
    switch (err) {
        case ERR_OK:
            return "Success";
        case ERR_INVALID_ARG:
            return "Invalid argument";
        case ERR_OUT_OF_MEMORY:
            return "Out of memory";
        case ERR_NOT_FOUND:
            return "Not found";
        case ERR_ALREADY_EXISTS:
            return "Already exists";
        case ERR_PERMISSION_DENIED:
            return "Permission denied";
        case ERR_NOT_SUPPORTED:
            return "Not supported";
        case ERR_TIMEOUT:
            return "Timeout";
        case ERR_INTERRUPTED:
            return "Interrupted";
        case ERR_INVALID_ADDRESS:
            return "Invalid address";
        case ERR_PAGE_FAULT:
            return "Page fault";
        case ERR_OUT_OF_PAGES:
            return "Out of pages";
        case ERR_MAPPING_FAILED:
            return "Mapping failed";
        case ERR_INVALID_PID:
            return "Invalid PID";
        case ERR_PROCESS_NOT_FOUND:
            return "Process not found";
        case ERR_PROCESS_ALREADY_RUNNING:
            return "Process already running";
        case ERR_CANNOT_CREATE_PROCESS:
            return "Cannot create process";
        case ERR_FILE_NOT_FOUND:
            return "File not found";
        case ERR_FILE_EXISTS:
            return "File exists";
        case ERR_DIRECTORY_NOT_FOUND:
            return "Directory not found";
        case ERR_NOT_A_DIRECTORY:
            return "Not a directory";
        case ERR_READ_ONLY:
            return "Read only";
        case ERR_DISK_FULL:
            return "Disk full";
        case ERR_IO_ERROR:
            return "I/O error";
        case ERR_DEVICE_NOT_FOUND:
            return "Device not found";
        case ERR_DEVICE_BUSY:
            return "Device busy";
        case ERR_INVALID_SYSCALL:
            return "Invalid syscall";
        case ERR_SYSCALL_FAILED:
            return "Syscall failed";
        case ERR_INVALID_ELF:
            return "Invalid ELF";
        case ERR_ELF_NOT_EXECUTABLE:
            return "ELF not executable";
        case ERR_ELF_LOAD_FAILED:
            return "ELF load failed";
        default:
            return "Unknown error";
    }
}

