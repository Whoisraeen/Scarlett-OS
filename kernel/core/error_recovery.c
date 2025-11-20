/**
 * @file error_recovery.c
 * @brief Error recovery implementation
 */

#include "../include/types.h"
#include "../include/error_recovery.h"
#include "../include/errors.h"
#include "../include/mm/heap.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

// Error recovery stack (per-thread, but simplified for now)
#define MAX_RECOVERY_STACK 16
static __thread error_recovery_ctx_t recovery_stack[MAX_RECOVERY_STACK];
static __thread int recovery_stack_top = -1;

/**
 * Push error recovery context
 */
error_recovery_ctx_t* error_recovery_push(error_code_t err, void* context, void (*cleanup)(void*)) {
    if (recovery_stack_top >= MAX_RECOVERY_STACK - 1) {
        kerror("Error recovery: Stack overflow\n");
        return NULL;
    }
    
    recovery_stack_top++;
    recovery_stack[recovery_stack_top].error = err;
    recovery_stack[recovery_stack_top].context = context;
    recovery_stack[recovery_stack_top].cleanup = cleanup;
    
    return &recovery_stack[recovery_stack_top];
}

/**
 * Pop error recovery context
 */
void error_recovery_pop(void) {
    if (recovery_stack_top < 0) {
        return;
    }
    
    recovery_stack_top--;
}

/**
 * Handle error with recovery
 */
error_code_t error_recovery_handle(error_code_t err) {
    if (!is_error(err)) {
        return err;
    }
    
    // Execute cleanup functions in reverse order
    while (recovery_stack_top >= 0) {
        error_recovery_ctx_t* ctx = &recovery_stack[recovery_stack_top];
        if (ctx->cleanup) {
            ctx->cleanup(ctx->context);
        }
        recovery_stack_top--;
    }
    
    return err;
}

/**
 * Cleanup all recovery contexts
 */
void error_recovery_cleanup_all(void) {
    while (recovery_stack_top >= 0) {
        error_recovery_ctx_t* ctx = &recovery_stack[recovery_stack_top];
        if (ctx->cleanup) {
            ctx->cleanup(ctx->context);
        }
        recovery_stack_top--;
    }
}

