/**
 * @file spinlock.h
 * @brief Spinlock synchronization primitives
 */

#ifndef KERNEL_SYNC_SPINLOCK_H
#define KERNEL_SYNC_SPINLOCK_H

#include "../types.h"

// Spinlock structure
typedef struct {
    volatile uint32_t locked;    // 0 = unlocked, 1 = locked
    uint32_t cpu_id;             // CPU that holds the lock (for debugging)
} spinlock_t;

// Initialize spinlock
#define SPINLOCK_INIT { .locked = 0, .cpu_id = 0 }

// Spinlock functions
void spinlock_init(spinlock_t* lock);
void spinlock_lock(spinlock_t* lock);
void spinlock_unlock(spinlock_t* lock);
bool spinlock_trylock(spinlock_t* lock);  // Try to acquire lock, return immediately if busy
bool spinlock_is_locked(spinlock_t* lock);

// Atomic operations
uint32_t atomic_add(volatile uint32_t* ptr, uint32_t value);
uint32_t atomic_sub(volatile uint32_t* ptr, uint32_t value);
uint32_t atomic_inc(volatile uint32_t* ptr);
uint32_t atomic_dec(volatile uint32_t* ptr);
uint32_t atomic_exchange(volatile uint32_t* ptr, uint32_t value);
bool atomic_compare_exchange(volatile uint32_t* ptr, uint32_t expected, uint32_t desired);

// Memory barriers
void memory_barrier(void);
void read_barrier(void);
void write_barrier(void);

#endif // KERNEL_SYNC_SPINLOCK_H

