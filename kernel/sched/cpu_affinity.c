/**
 * @file cpu_affinity.c
 * @brief CPU affinity management
 * 
 * Functions to set and get CPU affinity for threads.
 */

#include "../include/types.h"
#include "../include/sched/scheduler.h"
#include "../include/cpu.h"
#include "../include/errors.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/sync/spinlock.h"

// External thread table (defined in scheduler.c)
extern spinlock_t thread_table_lock;
extern thread_t* thread_table[];

/**
 * Set CPU affinity for a thread
 */
error_code_t thread_set_affinity(uint64_t tid, int32_t cpu_id) {
    if (tid == 0) {
        return ERR_INVALID_ARG;
    }
    
    // Validate CPU ID
    uint32_t num_cpus = cpu_get_count();
    if (cpu_id >= (int32_t)num_cpus && cpu_id != -1) {
        return ERR_INVALID_ARG;
    }
    
    // Find thread
    spinlock_lock(&thread_table_lock);
    thread_t* thread = thread_table[tid];
    if (!thread) {
        spinlock_unlock(&thread_table_lock);
        return ERR_INVALID_PID;
    }
    
    // Set affinity
    // DONE: cpu_affinity field added
    thread->cpu_affinity = cpu_id;
    spinlock_unlock(&thread_table_lock);
    
    kinfo("Thread %lu: CPU affinity set to %d\n", tid, cpu_id);
    return ERR_OK;
}

/**
 * Get CPU affinity for a thread
 */
int32_t thread_get_affinity(uint64_t tid) {
    if (tid == 0) {
        return -1;
    }
    
    spinlock_lock(&thread_table_lock);
    thread_t* thread = thread_table[tid];
    if (!thread) {
        spinlock_unlock(&thread_table_lock);
        return -1;
    }
    
    // DONE: cpu_affinity field added
    int32_t affinity = thread->cpu_affinity;
    spinlock_unlock(&thread_table_lock);
    
    return affinity;
}

/**
 * Set CPU affinity for current thread
 */
error_code_t thread_set_affinity_current(int32_t cpu_id) {
    thread_t* thread = thread_current();
    if (!thread) {
        return ERR_INVALID_ARG;
    }
    
    return thread_set_affinity(thread->tid, cpu_id);
}

/**
 * Get CPU affinity for current thread
 */
int32_t thread_get_affinity_current(void) {
    thread_t* thread = thread_current();
    if (!thread) {
        return -1;
    }
    
    // DONE: cpu_affinity field added
    return thread->cpu_affinity;
}

