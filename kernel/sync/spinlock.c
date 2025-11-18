/**
 * @file spinlock.c
 * @brief Spinlock implementation
 */

#include "../include/types.h"
#include "../include/sync/spinlock.h"
#include "../include/cpu.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

/**
 * Initialize spinlock
 */
void spinlock_init(spinlock_t* lock) {
    if (!lock) {
        return;
    }
    lock->locked = 0;
    lock->cpu_id = 0;
}

/**
 * Memory barrier
 */
void memory_barrier(void) {
    __asm__ volatile("mfence" ::: "memory");
}

/**
 * Read barrier
 */
void read_barrier(void) {
    __asm__ volatile("lfence" ::: "memory");
}

/**
 * Write barrier
 */
void write_barrier(void) {
    __asm__ volatile("sfence" ::: "memory");
}

/**
 * Atomic exchange
 */
static inline uint32_t xchg(volatile uint32_t* ptr, uint32_t value) {
    __asm__ volatile("xchgl %0, %1"
                     : "=r"(value), "+m"(*ptr)
                     : "0"(value)
                     : "memory");
    return value;
}

/**
 * Lock spinlock
 */
void spinlock_lock(spinlock_t* lock) {
    if (!lock) {
        return;
    }
    
    uint32_t cpu_id = cpu_get_current_id();
    
    // Try to acquire lock
    while (xchg(&lock->locked, 1) != 0) {
        // Lock is held, wait
        __asm__ volatile("pause");
    }
    
    // Memory barrier to ensure all previous operations complete
    memory_barrier();
    
    // Set CPU ID (for debugging)
    lock->cpu_id = cpu_id;
}

/**
 * Unlock spinlock
 */
void spinlock_unlock(spinlock_t* lock) {
    if (!lock) {
        return;
    }
    
    // Memory barrier before unlock
    memory_barrier();
    
    // Clear CPU ID
    lock->cpu_id = 0;
    
    // Release lock
    lock->locked = 0;
}

/**
 * Try to lock spinlock (non-blocking)
 */
bool spinlock_trylock(spinlock_t* lock) {
    if (!lock) {
        return false;
    }
    
    uint32_t old = xchg(&lock->locked, 1);
    if (old == 0) {
        // Got the lock
        memory_barrier();
        lock->cpu_id = cpu_get_current_id();
        return true;
    }
    
    return false;
}

/**
 * Check if spinlock is locked
 */
bool spinlock_is_locked(spinlock_t* lock) {
    if (!lock) {
        return false;
    }
    return lock->locked != 0;
}

/**
 * Atomic add
 */
uint32_t atomic_add(volatile uint32_t* ptr, uint32_t value) {
    return __sync_add_and_fetch(ptr, value);
}

/**
 * Atomic subtract
 */
uint32_t atomic_sub(volatile uint32_t* ptr, uint32_t value) {
    return __sync_sub_and_fetch(ptr, value);
}

/**
 * Atomic increment
 */
uint32_t atomic_inc(volatile uint32_t* ptr) {
    return atomic_add(ptr, 1);
}

/**
 * Atomic decrement
 */
uint32_t atomic_dec(volatile uint32_t* ptr) {
    return atomic_sub(ptr, 1);
}

/**
 * Atomic exchange
 */
uint32_t atomic_exchange(volatile uint32_t* ptr, uint32_t value) {
    return __sync_lock_test_and_set(ptr, value);
}

/**
 * Atomic compare and exchange
 */
bool atomic_compare_exchange(volatile uint32_t* ptr, uint32_t expected, uint32_t desired) {
    return __sync_bool_compare_and_swap(ptr, expected, desired);
}

