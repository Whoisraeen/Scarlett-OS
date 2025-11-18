/**
 * @file scheduler.c
 * @brief Thread scheduler implementation
 * 
 * SMP-aware scheduler with per-CPU runqueues.
 */

#include "../include/types.h"
#include "../include/sched/scheduler.h"
#include "../include/cpu.h"
#include "../include/sync/spinlock.h"
#include "../include/mm/heap.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/hal/timer.h"

// Forward declaration for our custom snprintf
int snprintf(char* buf, size_t size, const char* fmt, ...);

#define KERNEL_STACK_SIZE (64 * 1024)  // 64KB per thread
#define MAX_THREADS 256
#define MAX_CPUS 256

// Per-CPU runqueue structure
typedef struct {
    spinlock_t lock;                    // Lock for this runqueue
    thread_t* ready_queues[128];        // Ready queues (one per priority level)
    thread_t* blocked_queue;            // Blocked threads
    thread_t* current_thread;           // Currently running thread
    thread_t* idle_thread;             // Per-CPU idle thread
    uint32_t cpu_id;                   // CPU ID this runqueue belongs to
} per_cpu_runqueue_t;

// Global thread table (shared, protected by lock)
spinlock_t thread_table_lock = SPINLOCK_INIT;
thread_t* thread_table[MAX_THREADS];
static uint64_t next_tid = 1;

// Global sleeping queue
static spinlock_t sleeping_queue_lock = SPINLOCK_INIT;
static thread_t* sleeping_queue = NULL;

// Per-CPU runqueues
static per_cpu_runqueue_t per_cpu_runqueues[MAX_CPUS];

/**
 * Context switch (defined in assembly)
 */
extern void context_switch(cpu_context_t* old_ctx, cpu_context_t* new_ctx);

/**
 * Idle thread function
 */
static void idle_thread_func(void* arg) {
    (void)arg;
    
    while (1) {
        __asm__ volatile("hlt");
    }
}

/**
 * Get per-CPU runqueue for current CPU
 */
static per_cpu_runqueue_t* get_current_runqueue(void) {
    uint32_t cpu_id = cpu_get_current_id();
    if (cpu_id >= MAX_CPUS) {
        cpu_id = 0;  // Fallback to CPU 0
    }
    return &per_cpu_runqueues[cpu_id];
}

/**
 * Get per-CPU runqueue for specific CPU (for load balancing)
 */
per_cpu_runqueue_t* get_cpu_runqueue(uint32_t cpu_id) {
    if (cpu_id >= MAX_CPUS) {
        return NULL;
    }
    return &per_cpu_runqueues[cpu_id];
}

/**
 * Initialize idle thread for a CPU
 */
static void init_idle_thread_for_cpu(uint32_t cpu_id) {
    per_cpu_runqueue_t* rq = &per_cpu_runqueues[cpu_id];
    
    // Allocate idle thread
    thread_t* idle = (thread_t*)kmalloc(sizeof(thread_t));
    if (!idle) {
        kpanic("Failed to allocate idle thread for CPU");
    }
    
    // Initialize idle thread
    idle->tid = cpu_id;  // Use CPU ID as TID for idle threads
    snprintf(idle->name, sizeof(idle->name), "idle-%u", cpu_id);
    idle->state = THREAD_STATE_READY;
    idle->priority = THREAD_PRIORITY_IDLE;
    idle->kernel_stack = NULL;
    idle->kernel_stack_size = 0;
    idle->next = NULL;
    idle->cpu_time = 0;
    idle->wakeup_time = 0;
    
    rq->idle_thread = idle;
    rq->ready_queues[THREAD_PRIORITY_IDLE] = idle;
    rq->current_thread = idle;
}

/**
 * Initialize per-CPU scheduler (for APs)
 */
void scheduler_init_per_cpu(uint32_t cpu_id) {
    if (cpu_id >= MAX_CPUS) {
        return;
    }
    
    per_cpu_runqueue_t* rq = &per_cpu_runqueues[cpu_id];
    
    // Initialize lock
    spinlock_init(&rq->lock);
    
    // Clear ready queues
    for (int j = 0; j < 128; j++) {
        rq->ready_queues[j] = NULL;
    }
    
    rq->blocked_queue = NULL;
    rq->current_thread = NULL;
    rq->idle_thread = NULL;
    rq->cpu_id = cpu_id;
    
    // Initialize idle thread for this CPU
    init_idle_thread_for_cpu(cpu_id);
    
    kinfo("Scheduler initialized for CPU %u\n", cpu_id);
}

/**
 * Initialize scheduler
 */
void scheduler_init(void) {
    kinfo("Initializing SMP-aware scheduler...\n");
    
    // Initialize thread table lock
    spinlock_init(&thread_table_lock);
    
    // Clear thread table
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_table[i] = NULL;
    }
    
    // Initialize per-CPU runqueues
    uint32_t num_cpus = cpu_get_count();
    if (num_cpus == 0) {
        num_cpus = 1;  // At least BSP
    }
    
    for (uint32_t i = 0; i < num_cpus && i < MAX_CPUS; i++) {
        per_cpu_runqueue_t* rq = &per_cpu_runqueues[i];
        
        // Initialize lock
        spinlock_init(&rq->lock);
        
        // Clear ready queues
        for (int j = 0; j < 128; j++) {
            rq->ready_queues[j] = NULL;
        }
        
        rq->blocked_queue = NULL;
        rq->current_thread = NULL;
        rq->idle_thread = NULL;
        rq->cpu_id = i;
        
        // Initialize idle thread for this CPU
        init_idle_thread_for_cpu(i);
    }
    
    kinfo("Scheduler initialized for %u CPU(s)\n", num_cpus);
}

/**
 * Add thread to ready queue (on specified CPU)
 */
void add_to_ready_queue(thread_t* thread, uint32_t cpu_id) {
    if (cpu_id >= MAX_CPUS) {
        cpu_id = 0;  // Fallback
    }
    
    per_cpu_runqueue_t* rq = &per_cpu_runqueues[cpu_id];
    uint8_t priority = thread->priority;
    
    spinlock_lock(&rq->lock);
    
    if (rq->ready_queues[priority] == NULL) {
        rq->ready_queues[priority] = thread;
        thread->next = NULL;
    } else {
        // Add to end of queue
        thread_t* current = rq->ready_queues[priority];
        while (current->next) {
            current = current->next;
        }
        current->next = thread;
        thread->next = NULL;
    }
    
    spinlock_unlock(&rq->lock);
}

/**
 * Remove thread from ready queue (searches all CPUs)
 */
void remove_from_ready_queue(thread_t* thread) {
    uint8_t priority = thread->priority;
    
    // Search all CPU runqueues
    for (uint32_t i = 0; i < MAX_CPUS; i++) {
        per_cpu_runqueue_t* rq = &per_cpu_runqueues[i];
        
        spinlock_lock(&rq->lock);
        
        if (rq->ready_queues[priority] == thread) {
            rq->ready_queues[priority] = thread->next;
            spinlock_unlock(&rq->lock);
            return;
        }
        
        thread_t* current = rq->ready_queues[priority];
        while (current && current->next != thread) {
            current = current->next;
        }
        
        if (current) {
            current->next = thread->next;
            spinlock_unlock(&rq->lock);
            return;
        }
        
        spinlock_unlock(&rq->lock);
    }
}

/**
 * Pick next thread to run (on current CPU)
 */
static thread_t* pick_next_thread(void) {
    per_cpu_runqueue_t* rq = get_current_runqueue();
    uint32_t cpu_id = cpu_get_current_id();
    
    spinlock_lock(&rq->lock);
    
    // Find highest priority non-empty queue
    for (int i = 127; i >= 0; i--) {
        if (rq->ready_queues[i]) {
            thread_t* thread = rq->ready_queues[i];
            
            // Remove from front of queue
            rq->ready_queues[i] = thread->next;
            
            // Add back to end (round-robin within priority)
            if (thread != rq->idle_thread && thread->state == THREAD_STATE_READY) {
                thread->next = NULL;
                if (rq->ready_queues[i] == NULL) {
                    rq->ready_queues[i] = thread;
                } else {
                    thread_t* current = rq->ready_queues[i];
                    while (current->next) {
                        current = current->next;
                    }
                    current->next = thread;
                }
            }
            
            spinlock_unlock(&rq->lock);
            return thread;
        }
    }
    
    // No threads ready - try work stealing before going idle
    spinlock_unlock(&rq->lock);
    
    extern bool scheduler_try_work_stealing(uint32_t cpu_id);
    if (scheduler_try_work_stealing(cpu_id)) {
        // Work was stolen, try again
        spinlock_lock(&rq->lock);
        for (int i = 127; i >= 0; i--) {
            if (rq->ready_queues[i]) {
                thread_t* thread = rq->ready_queues[i];
                rq->ready_queues[i] = thread->next;
                thread->next = NULL;
                if (rq->ready_queues[i] == NULL) {
                    rq->ready_queues[i] = thread;
                } else {
                    thread_t* current = rq->ready_queues[i];
                    while (current->next) {
                        current = current->next;
                    }
                    current->next = thread;
                }
                spinlock_unlock(&rq->lock);
                return thread;
            }
        }
        spinlock_unlock(&rq->lock);
    }
    
    // Still nothing - return idle thread
    return rq->idle_thread;
}

/**
 * Thread entry wrapper
 */
static void thread_entry_wrapper(void (*entry)(void*), void* arg) {
    // Enable interrupts
    __asm__ volatile("sti");
    
    // Call thread function
    entry(arg);
    
    // Thread returned, exit
    thread_exit();
}

/**
 * Create a new thread
 */
uint64_t thread_create(void (*entry)(void*), void* arg, uint8_t priority, const char* name) {
    // Allocate thread structure
    thread_t* thread = (thread_t*)kmalloc(sizeof(thread_t));
    if (!thread) {
        return 0;
    }
    
    // Allocate kernel stack
    void* stack = kmalloc(KERNEL_STACK_SIZE);
    if (!stack) {
        kfree(thread);
        return 0;
    }
    
    // Assign TID
    uint64_t tid = next_tid++;
    if (tid >= MAX_THREADS) {
        kfree(stack);
        kfree(thread);
        return 0;
    }
    
      // Initialize thread
      thread->tid = tid;
      snprintf(thread->name, sizeof(thread->name), "%s", name ? name : "unnamed");
      thread->state = THREAD_STATE_READY;
      thread->priority = priority;
      thread->kernel_stack = stack;
      thread->kernel_stack_size = KERNEL_STACK_SIZE;
      thread->next = NULL;
      thread->cpu_time = 0;
      thread->wakeup_time = 0;
      // No CPU affinity set (can run on any CPU)
    
    // Set up initial stack frame
    uint64_t* stack_top = (uint64_t*)((uint8_t*)stack + KERNEL_STACK_SIZE);
    
    // Push arguments and return address
    *(--stack_top) = (uint64_t)arg;                       // arg
    *(--stack_top) = (uint64_t)entry;                     // entry
    *(--stack_top) = (uint64_t)thread_entry_wrapper;      // return address
    
    // Initialize context
    thread->context.rsp = (uint64_t)stack_top;
    thread->context.rip = (uint64_t)thread_entry_wrapper;
    thread->context.rflags = 0x202;  // IF enabled
    thread->context.cs = 0x08;       // Kernel code segment
    thread->context.ss = 0x10;       // Kernel data segment
    thread->context.rdi = (uint64_t)entry;
    thread->context.rsi = (uint64_t)arg;
    
    // Add to thread table (protected by lock)
    spinlock_lock(&thread_table_lock);
    thread_table[tid] = thread;
    spinlock_unlock(&thread_table_lock);
    
      // Add to ready queue on appropriate CPU
      uint32_t target_cpu = cpu_get_current_id();
      
      // Check CPU affinity (if implemented)
      // For now, all threads can run on any CPU
      target_cpu = cpu_get_current_id();
      
      add_to_ready_queue(thread, target_cpu);
    
    kinfo("Thread created: tid=%lu, name=%s, priority=%u\n", tid, thread->name, priority);
    
    return tid;
}

/**
 * Exit current thread
 */
void thread_exit(void) {
    per_cpu_runqueue_t* rq = get_current_runqueue();
    thread_t* thread = rq->current_thread;
    
    kinfo("Thread exiting: tid=%lu, name=%s\n", thread->tid, thread->name);
    
    thread->state = THREAD_STATE_DEAD;
    
    // Remove from thread table (protected by lock)
    spinlock_lock(&thread_table_lock);
    thread_table[thread->tid] = NULL;
    spinlock_unlock(&thread_table_lock);
    
    // Free resources (will be done by cleanup thread later)
    // For now, just mark as dead
    
    // Schedule next thread
    scheduler_schedule();
    
    // Should never return
    kpanic("thread_exit returned!");
}

/**
 * Yield CPU
 */
void thread_yield(void) {
    scheduler_schedule();
}

/**
 * Sleep for milliseconds
 */
void thread_sleep(uint64_t ms) {
    if (ms == 0) {
        thread_yield();
        return;
    }

    per_cpu_runqueue_t* rq = get_current_runqueue();
    thread_t* thread = rq->current_thread;

    // Calculate wakeup time
    uint64_t current_ticks = timer_get_ticks();
    uint64_t ticks_to_sleep = (ms * 100) / 1000; // 100 Hz timer
    if (ticks_to_sleep == 0) {
        ticks_to_sleep = 1;
    }
    thread->wakeup_time = current_ticks + ticks_to_sleep;
    
    // Set state to sleeping
    thread->state = THREAD_STATE_SLEEPING;

    // Add to global sleeping queue
    spinlock_lock(&sleeping_queue_lock);
    thread->next = sleeping_queue;
    sleeping_queue = thread;
    spinlock_unlock(&sleeping_queue_lock);

    // Yield CPU
    scheduler_schedule();
}

/**
 * Get current thread
 */
thread_t* thread_current(void) {
    per_cpu_runqueue_t* rq = get_current_runqueue();
    return rq->current_thread;
}

/**
 * Schedule next thread
 */
void scheduler_schedule(void) {
    per_cpu_runqueue_t* rq = get_current_runqueue();
    
    if (!rq->current_thread) {
        rq->current_thread = rq->idle_thread;
        return;
    }
    
    thread_t* old_thread = rq->current_thread;
    
    // If current thread is still runnable, add back to ready queue
    if (old_thread->state == THREAD_STATE_RUNNING) {
        old_thread->state = THREAD_STATE_READY;
        if (old_thread != rq->idle_thread) {
            uint32_t cpu_id = cpu_get_current_id();
            add_to_ready_queue(old_thread, cpu_id);
        }
    }
    
    // Pick next thread
    thread_t* new_thread = pick_next_thread();
    
    if (new_thread == old_thread) {
        // Same thread, just continue
        return;
    }
    
    new_thread->state = THREAD_STATE_RUNNING;
    rq->current_thread = new_thread;
    
    // Context switch
    context_switch(&old_thread->context, &new_thread->context);
}

// Per-CPU reschedule flags
static volatile bool need_reschedule[MAX_CPUS] = {false};

/**
 * Timer tick handler (called from timer interrupt)
 * 
 * This enables preemptive multitasking by marking that a reschedule
 * is needed. The actual context switch happens when returning from
 * the interrupt.
 */
void scheduler_tick(void) {
    per_cpu_runqueue_t* rq = get_current_runqueue();
    uint32_t cpu_id = cpu_get_current_id();
    
    // Increment current thread's CPU time
    if (rq->current_thread && rq->current_thread != rq->idle_thread) {
        rq->current_thread->cpu_time++;
    }

    // CPU 0 is responsible for waking up sleeping threads and load balancing
    if (cpu_id == 0) {
        uint64_t current_ticks = timer_get_ticks();
        spinlock_lock(&sleeping_queue_lock);

        thread_t* prev = NULL;
        thread_t* current = sleeping_queue;
        while (current) {
            if (current_ticks >= current->wakeup_time) {
                // Wake up thread
                thread_t* to_wake = current;
                
                // Remove from sleeping queue
                if (prev) {
                    prev->next = current->next;
                } else {
                    sleeping_queue = current->next;
                }
                current = current->next;

                // Add to its original CPU's ready queue
                to_wake->state = THREAD_STATE_READY;
                // For simplicity, we wake it up on the current CPU (CPU 0)
                // A more advanced implementation would store the original CPU
                add_to_ready_queue(to_wake, 0);

            } else {
                prev = current;
                current = current->next;
            }
        }
        spinlock_unlock(&sleeping_queue_lock);
        
        // Perform load balancing
        extern void scheduler_load_balance(void);
        scheduler_load_balance();
    }
    
    // Check if we should preempt (every 10 ticks = 10ms for 100Hz timer)
    static uint64_t tick_counter[MAX_CPUS] = {0};
    tick_counter[cpu_id]++;
    
    if (tick_counter[cpu_id] >= 10) {
        tick_counter[cpu_id] = 0;
        
        // Mark that we need to reschedule
        need_reschedule[cpu_id] = true;
    }
}

/**
 * Check if reschedule is needed and perform it
 * Called after returning from interrupt
 */
void scheduler_check_reschedule(void) {
    uint32_t cpu_id = cpu_get_current_id();
    if (need_reschedule[cpu_id]) {
        need_reschedule[cpu_id] = false;
        scheduler_schedule();
    }
}

/**
 * Block current thread
 */
void thread_block(void) {
    per_cpu_runqueue_t* rq = get_current_runqueue();
    thread_t* thread = rq->current_thread;
    thread->state = THREAD_STATE_BLOCKED;
    
    // Add to blocked queue
    spinlock_lock(&rq->lock);
    thread->next = rq->blocked_queue;
    rq->blocked_queue = thread;
    spinlock_unlock(&rq->lock);
    
    scheduler_schedule();
}

/**
 * Unblock a thread
 */
void thread_unblock(thread_t* thread) {
    if (thread->state != THREAD_STATE_BLOCKED) {
        return;
    }
    
    // Search all CPU runqueues for the blocked thread
    for (uint32_t i = 0; i < MAX_CPUS; i++) {
        per_cpu_runqueue_t* rq = &per_cpu_runqueues[i];
        
        spinlock_lock(&rq->lock);
        
        // Remove from blocked queue
        if (rq->blocked_queue == thread) {
            rq->blocked_queue = thread->next;
            spinlock_unlock(&rq->lock);
            
            // Add to ready queue on current CPU
            thread->state = THREAD_STATE_READY;
            uint32_t cpu_id = cpu_get_current_id();
            add_to_ready_queue(thread, cpu_id);
            return;
        }
        
        thread_t* current = rq->blocked_queue;
        while (current && current->next != thread) {
            current = current->next;
        }
        if (current) {
            current->next = thread->next;
            spinlock_unlock(&rq->lock);
            
            // Add to ready queue on current CPU
            thread->state = THREAD_STATE_READY;
            uint32_t cpu_id = cpu_get_current_id();
            add_to_ready_queue(thread, cpu_id);
            return;
        }
        
        spinlock_unlock(&rq->lock);
    }
}

// Simple snprintf implementation (add to kprintf.c later)
int snprintf(char* buf, size_t size, const char* fmt, ...) {
    if (size == 0) return 0;
    
    size_t i = 0;
    while (*fmt && i < size - 1) {
        buf[i++] = *fmt++;
    }
    buf[i] = '\0';
    
    return i;
}

