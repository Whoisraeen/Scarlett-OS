/**
 * @file work_stealing.c
 * @brief Work stealing for idle CPUs
 * 
 * When a CPU becomes idle, it can "steal" work from other CPUs' runqueues.
 */

#include "../include/types.h"
#include "../include/sched/scheduler.h"
#include "../include/cpu.h"
#include "../include/sync/spinlock.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

// Forward declaration
typedef struct per_cpu_runqueue {
    spinlock_t lock;
    thread_t* ready_queues[128];
    thread_t* blocked_queue;
    thread_t* current_thread;
    thread_t* idle_thread;
    uint32_t cpu_id;
} per_cpu_runqueue_t;

// Forward declaration
extern per_cpu_runqueue_t* get_cpu_runqueue(uint32_t cpu_id);
extern void add_to_ready_queue(thread_t* thread, uint32_t cpu_id);

/**
 * Try to steal a thread from another CPU's runqueue
 * @param thief_cpu_id The CPU that wants to steal work
 * @param victim_cpu_id The CPU to steal from
 * @return Pointer to stolen thread, or NULL if nothing to steal
 */
static thread_t* try_steal_from_cpu(uint32_t thief_cpu_id, uint32_t victim_cpu_id) {
    if (thief_cpu_id == victim_cpu_id) {
        return NULL;  // Can't steal from yourself
    }
    
    per_cpu_runqueue_t* victim_rq = get_cpu_runqueue(victim_cpu_id);
    if (!victim_rq) {
        return NULL;
    }
    
    // Try to acquire the victim's runqueue lock
    if (!spinlock_trylock(&victim_rq->lock)) {
        return NULL;  // Couldn't acquire lock, skip this CPU
    }
    
    // Look for a thread to steal (prefer lower priority to avoid disrupting high-priority work)
    thread_t* stolen = NULL;
    
    // Start from lower priorities and work up (steal less important work first)
    for (int priority = 0; priority < 64; priority++) {
        if (victim_rq->ready_queues[priority]) {
            stolen = victim_rq->ready_queues[priority];
            
            // Remove from victim's queue
            victim_rq->ready_queues[priority] = stolen->next;
            stolen->next = NULL;
            
            // Check CPU affinity (if implemented)
            // DONE: cpu_affinity field added
            // Check if thread has CPU affinity set
            if (stolen->cpu_affinity >= 0 && stolen->cpu_affinity != (int32_t)thief_cpu_id) {
                // Thread is bound to a different CPU, skip it and try next thread
                // Put it back and continue searching
                stolen->next = victim_rq->ready_queues[priority];
                victim_rq->ready_queues[priority] = stolen;
                stolen = NULL;
                break;  // Try next priority level
            }
            
            // Found a stealable thread
            break;
        }
    }
    
    spinlock_unlock(&victim_rq->lock);
    
    if (stolen) {
        kinfo("Work stealing: CPU %u stole thread %lu (priority %u) from CPU %u\n",
              thief_cpu_id, stolen->tid, stolen->priority, victim_cpu_id);
    }
    
    return stolen;
}

/**
 * Attempt work stealing when CPU becomes idle
 * @param idle_cpu_id The CPU that is idle and wants work
 * @return true if work was stolen, false otherwise
 */
bool scheduler_try_work_stealing(uint32_t idle_cpu_id) {
    uint32_t num_cpus = cpu_get_count();
    if (num_cpus <= 1) {
        return false;  // No other CPUs to steal from
    }
    
    // Try to steal from each CPU (starting from a random offset to avoid contention)
    // For simplicity, we'll start from CPU 0 and wrap around
    static uint32_t steal_start[MAX_CPUS] = {0};
    uint32_t start = steal_start[idle_cpu_id];
    
    for (uint32_t i = 0; i < num_cpus; i++) {
        uint32_t victim_cpu_id = (start + i) % num_cpus;
        
        if (victim_cpu_id == idle_cpu_id) {
            continue;  // Skip self
        }
        
        thread_t* stolen = try_steal_from_cpu(idle_cpu_id, victim_cpu_id);
        if (stolen) {
            // Add to thief's ready queue
            add_to_ready_queue(stolen, idle_cpu_id);
            
            // Update steal start for next time (round-robin)
            steal_start[idle_cpu_id] = (victim_cpu_id + 1) % num_cpus;
            
            return true;  // Successfully stole work
        }
    }
    
    // Update steal start for next time
    steal_start[idle_cpu_id] = (start + 1) % num_cpus;
    
    return false;  // No work to steal
}

