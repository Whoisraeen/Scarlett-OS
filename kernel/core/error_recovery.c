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
#include "../sched/sched_o1.h" // Access task_t

// Access current task from scheduler
extern task_t* sched_get_current_task(void);

// Helper to get current task's recovery stack
static error_recovery_ctx_t* get_task_recovery_stack(task_t* task) {
    return (error_recovery_ctx_t*)task->recovery_stack;
}

static int* get_task_recovery_top(task_t* task) {
    return &task->recovery_stack_top;
}

/**
 * Push error recovery context
 */
error_recovery_ctx_t* error_recovery_push(error_code_t err, void* context, void (*cleanup)(void*)) {
    task_t* task = sched_get_current_task();
    if (!task) return NULL; // Early boot or no task
    
    int* top = get_task_recovery_top(task);
    if (*top >= MAX_RECOVERY_STACK - 1) {
        kerror("Error recovery: Stack overflow for task %d\n", task->tid);
        return NULL;
    }
    
    (*top)++;
    error_recovery_ctx_t* stack = get_task_recovery_stack(task);
    stack[*top].error = err;
    stack[*top].context = context;
    stack[*top].cleanup = cleanup;
    
    return &stack[*top];
}

/**
 * Pop error recovery context
 */
void error_recovery_pop(void) {
    task_t* task = sched_get_current_task();
    if (!task) return;
    
    int* top = get_task_recovery_top(task);
    if (*top < 0) return;
    
    (*top)--;
}

/**
 * Handle error with recovery
 */
error_code_t error_recovery_handle(error_code_t err) {
    if (!is_error(err)) return err;
    
    task_t* task = sched_get_current_task();
    if (!task) return err;
    
    int* top = get_task_recovery_top(task);
    error_recovery_ctx_t* stack = get_task_recovery_stack(task);
    
    // Execute cleanup functions in reverse order
    while (*top >= 0) {
        error_recovery_ctx_t* ctx = &stack[*top];
        if (ctx->cleanup) {
            ctx->cleanup(ctx->context);
        }
        (*top)--;
    }
    
    return err;
}

/**
 * Cleanup all recovery contexts
 */
void error_recovery_cleanup_all(void) {
    task_t* task = sched_get_current_task();
    if (!task) return;
    
    int* top = get_task_recovery_top(task);
    error_recovery_ctx_t* stack = get_task_recovery_stack(task);
    
    while (*top >= 0) {
        error_recovery_ctx_t* ctx = &stack[*top];
        if (ctx->cleanup) {
            ctx->cleanup(ctx->context);
        }
        (*top)--;
    }
}

