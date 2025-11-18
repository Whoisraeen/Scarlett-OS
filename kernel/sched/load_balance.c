/**
 * @file load_balance.c
 * @brief Load balancing for multi-core scheduler
 */

#include "../include/types.h"
#include "../include/sched/scheduler.h"
#include "../include/cpu.h"
#include "../include/sync/spinlock.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

// Load balancing interval (in scheduler ticks)
#define LOAD_BALANCE_INTERVAL 100  // Every 100 ticks = 1 second at 100Hz

// Load balancing threshold (difference in runqueue length)
#define LOAD_BALANCE_THRESHOLD 2

/**
 * Get runqueue length for a CPU
 */
static uint32_t get_runqueue_length(uint32_t cpu_id) {
    extern per_cpu_runqueue_t* get_cpu_runqueue(uint32_t cpu_id);
    per_cpu_runqueue_t* rq = get_cpu_runqueue(cpu_id);
    if (!rq) {
        return 0;
    }
    
    uint32_t count = 0;
    spinlock_lock(&rq->lock);
    
    for (int i = 0; i < 128; i++) {
        thread_t* thread = rq->ready_queues[i];
        while (thread) {
            count++;
            thread = thread->next;
        }
    }
    
    spinlock_unlock(&rq->lock);
    return count;
}

/**
 * Move thread from one CPU to another
 */
static void move_thread_to_cpu(thread_t* thread, uint32_t target_cpu_id) {
    if (!thread) {
        return;
    }
    
    // Remove from current CPU
    remove_from_ready_queue(thread);
    
    // Add to target CPU
    add_to_ready_queue(thread, target_cpu_id);
}

/**
 * Perform load balancing
 */
void scheduler_load_balance(void) {
    uint32_t num_cpus = cpu_get_count();
    if (num_cpus <= 1) {
        return;  // No balancing needed for single CPU
    }
    
    static uint64_t last_balance = 0;
    extern uint64_t timer_get_ticks(void);
    uint64_t current_ticks = timer_get_ticks();
    
    // Check if it's time to balance
    if (current_ticks - last_balance < LOAD_BALANCE_INTERVAL) {
        return;
    }
    
    last_balance = current_ticks;
    
    // Find busiest and least busy CPUs
    uint32_t busiest_cpu = 0;
    uint32_t least_busy_cpu = 0;
    uint32_t busiest_count = 0;
    uint32_t least_busy_count = UINT32_MAX;
    
    for (uint32_t i = 0; i < num_cpus; i++) {
        uint32_t count = get_runqueue_length(i);
        if (count > busiest_count) {
            busiest_count = count;
            busiest_cpu = i;
        }
        if (count < least_busy_count) {
            least_busy_count = count;
            least_busy_cpu = i;
        }
    }
    
    // Check if balancing is needed
    if (busiest_count - least_busy_count < LOAD_BALANCE_THRESHOLD) {
        return;  // Load is balanced
    }
    
    // Move threads from busiest to least busy
    per_cpu_runqueue_t* busiest_rq = get_cpu_runqueue(busiest_cpu);
    if (!busiest_rq) {
        return;
    }
    
    spinlock_lock(&busiest_rq->lock);
    
    // Find a thread to move (prefer lower priority to avoid disrupting high-priority work)
    for (int i = 0; i < 64; i++) {  // Only move normal/low priority threads
        if (busiest_rq->ready_queues[i]) {
            thread_t* thread = busiest_rq->ready_queues[i];
            busiest_rq->ready_queues[i] = thread->next;
            thread->next = NULL;
            
            spinlock_unlock(&busiest_rq->lock);
            
            // Move to least busy CPU
            move_thread_to_cpu(thread, least_busy_cpu);
            
            kinfo("Load balance: Moved thread %lu from CPU %u to CPU %u\n",
                  thread->tid, busiest_cpu, least_busy_cpu);
            return;
        }
    }
    
    spinlock_unlock(&busiest_rq->lock);
}

