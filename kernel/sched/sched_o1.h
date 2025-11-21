/**
 * @file sched_o1.h
 * @brief O(1) Scheduler Implementation
 *
 * Improved scheduler with constant-time operations and per-CPU run queues
 */

#ifndef KERNEL_SCHED_O1_H
#define KERNEL_SCHED_O1_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_PRIORITY 140
#define MAX_RT_PRIORITY 100
#define DEFAULT_PRIORITY 120
#define MAX_CPUS 256

// Task states
typedef enum {
    TASK_RUNNING = 0,
    TASK_READY,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_ZOMBIE,
} task_state_t;

// Task structure (simplified)
typedef struct task {
    uint32_t pid;
    uint32_t priority;
    uint32_t time_slice;
    uint32_t cpu;
    task_state_t state;
    
    // Scheduling info
    uint64_t vruntime;  // Virtual runtime
    uint32_t load_weight;
    
    // Links for run queue
    struct task* next;
    struct task* prev;
} task_t;

// Priority queue (linked list)
typedef struct {
    task_t* head;
    task_t* tail;
    uint32_t count;
} prio_queue_t;

// Per-CPU run queue
typedef struct {
    prio_queue_t active[MAX_PRIORITY];
    prio_queue_t expired[MAX_PRIORITY];
    
    uint64_t priority_bitmap[2];  // Bitmap for quick priority lookup
    
    task_t* current;
    task_t* idle_task;
    
    uint32_t nr_running;
    uint64_t load;
    
    uint32_t cpu_id;
    
    // Lock for this run queue
    void* lock;
} cpu_runqueue_t;

// Global scheduler state
typedef struct {
    cpu_runqueue_t per_cpu_rq[MAX_CPUS];
    uint32_t num_cpus;
    
    // Load balancing
    uint64_t last_balance_time;
    uint32_t balance_interval_ms;
    
    bool initialized;
} scheduler_t;

// Initialize scheduler
int sched_o1_init(uint32_t num_cpus);
void sched_o1_cleanup(void);

// Task management
void sched_add_task(task_t* task);
void sched_remove_task(task_t* task);
void sched_wake_task(task_t* task);
void sched_block_task(task_t* task);

// Scheduling
task_t* sched_pick_next_task(uint32_t cpu);
void sched_switch_to(task_t* next);
void sched_yield(void);

// Priority management
void sched_set_priority(task_t* task, uint32_t priority);
uint32_t sched_get_priority(task_t* task);

// Load balancing
void sched_balance_cpus(void);
void sched_migrate_task(task_t* task, uint32_t target_cpu);

// Statistics
uint32_t sched_get_nr_running(uint32_t cpu);
uint64_t sched_get_cpu_load(uint32_t cpu);

// Internal helpers
static inline int find_first_bit(uint64_t bitmap);
static inline void set_bit(uint64_t* bitmap, int bit);
static inline void clear_bit(uint64_t* bitmap, int bit);

#endif // KERNEL_SCHED_O1_H
