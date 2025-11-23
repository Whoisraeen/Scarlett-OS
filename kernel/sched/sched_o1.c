/**
 * @file sched_o1.c
 * @brief O(1) Scheduler Implementation
 */

#include "sched_o1.h"
#include "../include/sync/spinlock.h"
#include "../include/process/process.h"
#include "../include/mm/heap.h"
#include <string.h>
#include <stdio.h>

static scheduler_t global_sched;

// Bit manipulation helpers
static inline int find_first_bit(uint64_t bitmap) {
    if (bitmap == 0) return -1;
    return __builtin_ffsll(bitmap) - 1;
}

static inline void set_bit(uint64_t* bitmap, int bit) {
    int idx = bit / 64;
    int offset = bit % 64;
    bitmap[idx] |= (1ULL << offset);
}

static inline void clear_bit(uint64_t* bitmap, int bit) {
    int idx = bit / 64;
    int offset = bit % 64;
    bitmap[idx] &= ~(1ULL << offset);
}

static inline bool test_bit(uint64_t* bitmap, int bit) {
    int idx = bit / 64;
    int offset = bit % 64;
    return (bitmap[idx] & (1ULL << offset)) != 0;
}

// Initialize scheduler
int sched_o1_init(uint32_t num_cpus) {
    if (num_cpus > MAX_CPUS) {
        return -1;
    }
    
    memset(&global_sched, 0, sizeof(scheduler_t));
    global_sched.num_cpus = num_cpus;
    global_sched.balance_interval_ms = 100;  // Balance every 100ms
    
    // Initialize per-CPU run queues
    for (uint32_t cpu = 0; cpu < num_cpus; cpu++) {
        cpu_runqueue_t* rq = &global_sched.per_cpu_rq[cpu];
        rq->cpu_id = cpu;
        rq->nr_running = 0;
        rq->load = 0;
        rq->current = NULL;
        
        // Create idle task for this CPU
        task_t* idle = (task_t*)kmalloc(sizeof(task_t));
        if (idle) {
            memset(idle, 0, sizeof(task_t));
            idle->pid = 0;  // Idle task has PID 0
            idle->priority = MAX_PRIORITY - 1;  // Lowest priority
            idle->cpu = cpu;
            idle->state = TASK_READY;
            idle->time_slice = 1;
            idle->vruntime = 0;
            idle->load_weight = 1;
            idle->next = NULL;
            idle->prev = NULL;
            rq->idle_task = idle;
        } else {
            rq->idle_task = NULL;
        }
        
        // Initialize priority queues
        for (int prio = 0; prio < MAX_PRIORITY; prio++) {
            rq->active[prio].head = NULL;
            rq->active[prio].tail = NULL;
            rq->active[prio].count = 0;
            
            rq->expired[prio].head = NULL;
            rq->expired[prio].tail = NULL;
            rq->expired[prio].count = 0;
        }
        
        rq->priority_bitmap[0] = 0;
        rq->priority_bitmap[1] = 0;
        
        // Initialize spinlock for this run queue
        spinlock_t* lock = (spinlock_t*)kmalloc(sizeof(spinlock_t));
        if (lock) {
            spinlock_init(lock);
            rq->lock = lock;
        } else {
            rq->lock = NULL;
        }
    }
    
    global_sched.initialized = true;
    return 0;
}

void sched_o1_cleanup(void) {
    global_sched.initialized = false;
}

// Add task to run queue
static void enqueue_task(cpu_runqueue_t* rq, task_t* task, bool active) {
    prio_queue_t* queue_array = active ? rq->active : rq->expired;
    prio_queue_t* queue = &queue_array[task->priority];
    
    // Add to tail of priority queue
    task->next = NULL;
    task->prev = queue->tail;
    
    if (queue->tail) {
        queue->tail->next = task;
    } else {
        queue->head = task;
    }
    
    queue->tail = task;
    queue->count++;
    
    // Update priority bitmap
    set_bit(rq->priority_bitmap, task->priority);
    
    rq->nr_running++;
}

// Remove task from run queue
static void dequeue_task(cpu_runqueue_t* rq, task_t* task) {
    // Find which queue the task is in
    for (int active = 0; active <= 1; active++) {
        prio_queue_t* queue_array = active ? rq->active : rq->expired;
        prio_queue_t* queue = &queue_array[task->priority];
        
        // Remove from linked list
        if (task->prev) {
            task->prev->next = task->next;
        } else {
            queue->head = task->next;
        }
        
        if (task->next) {
            task->next->prev = task->prev;
        } else {
            queue->tail = task->prev;
        }
        
        queue->count--;
        
        // Clear bitmap if queue is empty
        if (queue->count == 0) {
            clear_bit(rq->priority_bitmap, task->priority);
        }
        
        rq->nr_running--;
        break;
    }
}

void sched_add_task(task_t* task) {
    if (!global_sched.initialized) return;
    
    // Assign to CPU (simple round-robin for now)
    static uint32_t next_cpu = 0;
    task->cpu = next_cpu % global_sched.num_cpus;
    next_cpu++;
    
    cpu_runqueue_t* rq = &global_sched.per_cpu_rq[task->cpu];
    
    // Acquire lock
    if (rq->lock) {
        spinlock_lock((spinlock_t*)rq->lock);
    }
    enqueue_task(rq, task, true);
    // Release lock
    if (rq->lock) {
        spinlock_unlock((spinlock_t*)rq->lock);
    }
}

void sched_remove_task(task_t* task) {
    if (!global_sched.initialized) return;
    
    cpu_runqueue_t* rq = &global_sched.per_cpu_rq[task->cpu];
    
    // Acquire lock
    if (rq->lock) {
        spinlock_lock((spinlock_t*)rq->lock);
    }
    dequeue_task(rq, task);
    // Release lock
    if (rq->lock) {
        spinlock_unlock((spinlock_t*)rq->lock);
    }
}

// Pick next task to run (O(1) operation)
task_t* sched_pick_next_task(uint32_t cpu) {
    if (!global_sched.initialized || cpu >= global_sched.num_cpus) {
        return NULL;
    }
    
    cpu_runqueue_t* rq = &global_sched.per_cpu_rq[cpu];
    
    // Find highest priority task in active queues
    int prio = find_first_bit(rq->priority_bitmap[0]);
    if (prio < 0 && rq->priority_bitmap[1] != 0) {
        prio = 64 + find_first_bit(rq->priority_bitmap[1]);
    }
    
    if (prio < 0) {
        // No tasks in active queues, swap active and expired
        prio_queue_t* temp = rq->active;
        rq->active = rq->expired;
        rq->expired = temp;
        
        // Recalculate bitmap
        rq->priority_bitmap[0] = 0;
        rq->priority_bitmap[1] = 0;
        
        for (int p = 0; p < MAX_PRIORITY; p++) {
            if (rq->active[p].count > 0) {
                set_bit(rq->priority_bitmap, p);
            }
        }
        
        prio = find_first_bit(rq->priority_bitmap[0]);
        if (prio < 0 && rq->priority_bitmap[1] != 0) {
            prio = 64 + find_first_bit(rq->priority_bitmap[1]);
        }
    }
    
    if (prio < 0) {
        // Still no tasks, return idle task
        return rq->idle_task;
    }
    
    // Get first task from priority queue
    prio_queue_t* queue = &rq->active[prio];
    task_t* task = queue->head;
    
    if (task) {
        // Remove from queue
        dequeue_task(rq, task);
        
        // Task will be re-enqueued after time slice expires
        task->state = TASK_RUNNING;
        rq->current = task;
    }
    
    return task;
}

void sched_set_priority(task_t* task, uint32_t priority) {
    if (priority >= MAX_PRIORITY) {
        priority = MAX_PRIORITY - 1;
    }
    
    cpu_runqueue_t* rq = &global_sched.per_cpu_rq[task->cpu];
    
    // Acquire lock
    if (rq->lock) {
        spinlock_lock((spinlock_t*)rq->lock);
    }
    
    // Remove from current priority queue
    dequeue_task(rq, task);
    
    // Update priority
    task->priority = priority;
    
    // Re-enqueue with new priority
    enqueue_task(rq, task, true);
    
    // Release lock
    if (rq->lock) {
        spinlock_unlock((spinlock_t*)rq->lock);
    }
}

// Load balancing
void sched_balance_cpus(void) {
    if (!global_sched.initialized) return;
    
    // Calculate average load
    uint64_t total_load = 0;
    for (uint32_t cpu = 0; cpu < global_sched.num_cpus; cpu++) {
        total_load += global_sched.per_cpu_rq[cpu].nr_running;
    }
    
    uint64_t avg_load = total_load / global_sched.num_cpus;
    uint32_t threshold = 2;  // Migrate if difference > 2 tasks
    
    // Find overloaded and underloaded CPUs
    for (uint32_t cpu = 0; cpu < global_sched.num_cpus; cpu++) {
        cpu_runqueue_t* rq = &global_sched.per_cpu_rq[cpu];
        
        if (rq->nr_running > avg_load + threshold) {
            // This CPU is overloaded, migrate tasks
            // TODO: Implement task migration
        }
    }
}

uint32_t sched_get_nr_running(uint32_t cpu) {
    if (cpu >= global_sched.num_cpus) return 0;
    return global_sched.per_cpu_rq[cpu].nr_running;
}

uint64_t sched_get_cpu_load(uint32_t cpu) {
    if (cpu >= global_sched.num_cpus) return 0;
    return global_sched.per_cpu_rq[cpu].load;
}

// Migrate task to different CPU
void sched_migrate_task(task_t* task, uint32_t target_cpu) {
    if (!task || target_cpu >= global_sched.num_cpus) return;
    
    uint32_t old_cpu = task->cpu;
    if (old_cpu == target_cpu) return;  // Already on target CPU
    
    cpu_runqueue_t* old_rq = &global_sched.per_cpu_rq[old_cpu];
    cpu_runqueue_t* new_rq = &global_sched.per_cpu_rq[target_cpu];
    
    // Acquire both locks (always in same order to avoid deadlock)
    if (old_cpu < target_cpu) {
        if (old_rq->lock) spinlock_lock((spinlock_t*)old_rq->lock);
        if (new_rq->lock) spinlock_lock((spinlock_t*)new_rq->lock);
    } else {
        if (new_rq->lock) spinlock_lock((spinlock_t*)new_rq->lock);
        if (old_rq->lock) spinlock_lock((spinlock_t*)old_rq->lock);
    }
    
    // Remove from old run queue
    dequeue_task(old_rq, task);
    
    // Update task CPU
    task->cpu = target_cpu;
    
    // Add to new run queue
    enqueue_task(new_rq, task, true);
    
    // Release locks
    if (old_rq->lock) spinlock_unlock((spinlock_t*)old_rq->lock);
    if (new_rq->lock) spinlock_unlock((spinlock_t*)new_rq->lock);
}
