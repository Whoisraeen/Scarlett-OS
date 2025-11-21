/**
 * @file types.h
 * @brief ScarlettOS Type Definitions
 */

#ifndef SCARLETTOS_TYPES_H
#define SCARLETTOS_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Process and thread types
typedef uint32_t pid_t;
typedef uint32_t tid_t;

// File system types
typedef uint64_t inode_t;
typedef int32_t fd_t;

// IPC types
typedef uint32_t port_t;
typedef uint64_t msg_id_t;

// Capability types
typedef uint64_t cap_t;

// Error codes
typedef enum {
    ERR_SUCCESS = 0,
    ERR_INVALID_PARAM = -1,
    ERR_NO_MEMORY = -2,
    ERR_NOT_FOUND = -3,
    ERR_PERMISSION_DENIED = -4,
    ERR_ALREADY_EXISTS = -5,
    ERR_TIMEOUT = -6,
    ERR_NOT_IMPLEMENTED = -7,
    ERR_IO_ERROR = -8,
    ERR_NETWORK_ERROR = -9,
} error_t;

// Result type
#define RESULT_OK(val) ((result_t){.success = true, .value = (val)})
#define RESULT_ERR(err) ((result_t){.success = false, .error = (err)})

typedef struct {
    bool success;
    union {
        uint64_t value;
        error_t error;
    };
} result_t;

#endif // SCARLETTOS_TYPES_H
