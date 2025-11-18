/**
 * @file rwlock.c
 * @brief Read-write lock implementation
 */

#include "../include/sync/rwlock.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

/**
 * Initialize a read-write lock
 */
void rwlock_init(rwlock_t* rwlock) {
    if (!rwlock) {
        return;
    }
    
    spinlock_init(&rwlock->lock);
    rwlock->readers = 0;
    rwlock->writer = false;
    rwlock->waiting_writers = 0;
}

/**
 * Acquire read lock (multiple readers allowed)
 */
void rwlock_read_lock(rwlock_t* rwlock) {
    if (!rwlock) {
        return;
    }
    
    spinlock_lock(&rwlock->lock);
    
    // Wait for writer to finish
    while (rwlock->writer || rwlock->waiting_writers > 0) {
        spinlock_unlock(&rwlock->lock);
        __asm__ volatile("pause");
        spinlock_lock(&rwlock->lock);
    }
    
    rwlock->readers++;
    spinlock_unlock(&rwlock->lock);
}

/**
 * Release read lock
 */
void rwlock_read_unlock(rwlock_t* rwlock) {
    if (!rwlock) {
        return;
    }
    
    spinlock_lock(&rwlock->lock);
    rwlock->readers--;
    spinlock_unlock(&rwlock->lock);
}

/**
 * Acquire write lock (exclusive)
 */
void rwlock_write_lock(rwlock_t* rwlock) {
    if (!rwlock) {
        return;
    }
    
    spinlock_lock(&rwlock->lock);
    
    // Increment waiting writers
    rwlock->waiting_writers++;
    
    // Wait for readers and other writers to finish
    while (rwlock->readers > 0 || rwlock->writer) {
        spinlock_unlock(&rwlock->lock);
        __asm__ volatile("pause");
        spinlock_lock(&rwlock->lock);
    }
    
    rwlock->waiting_writers--;
    rwlock->writer = true;
    spinlock_unlock(&rwlock->lock);
}

/**
 * Release write lock
 */
void rwlock_write_unlock(rwlock_t* rwlock) {
    if (!rwlock) {
        return;
    }
    
    spinlock_lock(&rwlock->lock);
    rwlock->writer = false;
    spinlock_unlock(&rwlock->lock);
}

