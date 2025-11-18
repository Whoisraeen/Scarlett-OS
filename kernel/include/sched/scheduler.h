/**
 * @file scheduler.h
 * @brief Thread scheduler interface
 */

#ifndef KERNEL_SCHED_SCHEDULER_H
#define KERNEL_SCHED_SCHEDULER_H

#include "../types.h"
#include "../sync/spinlock.h"

// Thread states
typedef enum {
    THREAD_STATE_READY,
    THREAD_STATE_RUNNING,
    THREAD_STATE_BLOCKED,
    THREAD_STATE_SLEEPING,
    THREAD_STATE_DEAD
} thread_state_t;

// Thread priority levels
#define THREAD_PRIORITY_IDLE 0
#define THREAD_PRIORITY_LOW 32
#define THREAD_PRIORITY_NORMAL 64
#define THREAD_PRIORITY_HIGH 96
#define THREAD_PRIORITY_REALTIME 127

// CPU context (saved registers)
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) cpu_context_t;

// Thread control block
typedef struct thread {
    uint64_t tid;                    // Thread ID
    char name[32];                   // Thread name
    thread_state_t state;            // Current state
    uint8_t priority;                // Priority (0-127)
    cpu_context_t context;           // Saved CPU state
    void* kernel_stack;              // Kernel stack
    size_t kernel_stack_size;        // Stack size
    struct thread* next;             // Next in queue
    uint64_t cpu_time;               // Total CPU time
    uint64_t wakeup_time;            // For sleeping threads
} thread_t;

/**
 * Initialize scheduler
 */
void scheduler_init(void);

/**
 * Initialize per-CPU scheduler (for Application Processors)
 */
void scheduler_init_per_cpu(uint32_t cpu_id);

/**
 * Create a new thread
 * @param entry Entry point function
 * @param arg Argument to pass to entry point
 * @param priority Thread priority
 * @param name Thread name
 * @return Thread ID or 0 on error
 */
uint64_t thread_create(void (*entry)(void*), void* arg, uint8_t priority, const char* name);

/**
 * Exit current thread
 */
void thread_exit(void) __attribute__((noreturn));

/**
 * Yield CPU to another thread
 */
void thread_yield(void);

/**
 * Sleep for milliseconds
 */
void thread_sleep(uint64_t ms);

/**
 * Get current thread
 */
thread_t* thread_current(void);

/**
 * Schedule next thread (called from timer interrupt)
 */
void scheduler_schedule(void);

/**
 * Timer tick handler (called from timer interrupt for preemptive scheduling)
 */
void scheduler_tick(void);

/**
 * Check if reschedule is needed and perform it
 * Called after returning from interrupt
 */
void scheduler_check_reschedule(void);

/**
 * Perform load balancing across CPUs
 */
void scheduler_load_balance(void);

/**
 * Attempt work stealing when CPU becomes idle
 * @param idle_cpu_id The CPU that is idle and wants work
 * @return true if work was stolen, false otherwise
 */
bool scheduler_try_work_stealing(uint32_t idle_cpu_id);

/**
 * Block current thread
 */
void thread_block(void);

/**
 * Unblock a thread
 */
void thread_unblock(thread_t* thread);

// Global thread table (for CPU affinity and other uses)
#define MAX_THREADS 256
extern spinlock_t thread_table_lock;
extern thread_t* thread_table[MAX_THREADS];

#endif // KERNEL_SCHED_SCHEDULER_H

