/**
 * @file error_recovery.h
 * @brief Error recovery mechanisms
 */

#ifndef KERNEL_ERROR_RECOVERY_H
#define KERNEL_ERROR_RECOVERY_H

#include "../types.h"
#include "../errors.h"

// Error recovery context
typedef struct {
    error_code_t error;         // The error that occurred
    void* context;              // Context for recovery
    void (*cleanup)(void*);     // Cleanup function
} error_recovery_ctx_t;

// Error recovery functions
error_recovery_ctx_t* error_recovery_push(error_code_t err, void* context, void (*cleanup)(void*));
void error_recovery_pop(void);
error_code_t error_recovery_handle(error_code_t err);
void error_recovery_cleanup_all(void);

// Automatic cleanup macros
#define ERROR_RECOVERY_BEGIN() \
    do { \
        error_recovery_ctx_t* __ctx = NULL

#define ERROR_RECOVERY_ON_ERROR(err, cleanup_fn) \
        if (is_error(err)) { \
            __ctx = error_recovery_push(err, NULL, cleanup_fn); \
            goto __error_label; \
        }

#define ERROR_RECOVERY_END() \
        if (__ctx) error_recovery_pop(); \
    } while (0); \
    __error_label: if (__ctx) { error_recovery_cleanup_all(); error_recovery_pop(); }

#endif // KERNEL_ERROR_RECOVERY_H

