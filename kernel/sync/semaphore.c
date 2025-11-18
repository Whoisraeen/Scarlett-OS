/**
 * @file semaphore.c
 * @brief Semaphore implementation
 */

#include "../include/types.h"
#include "../include/sync/semaphore.h"
#include "../include/sched/scheduler.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

/**
 * Initialize semaphore
 */
void semaphore_init(semaphore_t* sem, uint32_t initial_count, uint32_t max_count) {
    if (!sem) {
        return;
    }
    
    spinlock_init(&sem->lock);
    sem->count = initial_count;
    sem->max_count = max_count;
    sem->wait_count = 0;
}

/**
 * Wait on semaphore (decrement)
 */
void semaphore_wait(semaphore_t* sem) {
    if (!sem) {
        return;
    }
    
    while (1) {
        spinlock_lock(&sem->lock);
        
        if (sem->count > 0) {
            // Got the semaphore
            sem->count--;
            spinlock_unlock(&sem->lock);
            return;
        }
        
        // Semaphore is zero, wait
        sem->wait_count++;
        spinlock_unlock(&sem->lock);
        
        // Yield CPU (in a real OS, we'd block the thread)
        extern void thread_yield(void);
        thread_yield();
        
        // Decrement wait count when we wake up
        spinlock_lock(&sem->lock);
        sem->wait_count--;
        spinlock_unlock(&sem->lock);
    }
}

/**
 * Signal semaphore (increment)
 */
void semaphore_signal(semaphore_t* sem) {
    if (!sem) {
        return;
    }
    
    spinlock_lock(&sem->lock);
    
    if (sem->count < sem->max_count) {
        sem->count++;
    }
    
    spinlock_unlock(&sem->lock);
}

/**
 * Try to wait on semaphore (non-blocking)
 */
bool semaphore_trywait(semaphore_t* sem) {
    if (!sem) {
        return false;
    }
    
    spinlock_lock(&sem->lock);
    
    if (sem->count > 0) {
        sem->count--;
        spinlock_unlock(&sem->lock);
        return true;
    }
    
    spinlock_unlock(&sem->lock);
    return false;
}

/**
 * Get semaphore count
 */
uint32_t semaphore_get_count(semaphore_t* sem) {
    if (!sem) {
        return 0;
    }
    
    spinlock_lock(&sem->lock);
    uint32_t count = sem->count;
    spinlock_unlock(&sem->lock);
    
    return count;
}

