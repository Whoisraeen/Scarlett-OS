/**
 * @file rwlock.h
 * @brief Read-write lock implementation
 * 
 * Read-write locks allow multiple readers or a single writer.
 */

#ifndef KERNEL_SYNC_RWLOCK_H
#define KERNEL_SYNC_RWLOCK_H

#include "../types.h"
#include "spinlock.h"

// Read-write lock structure
typedef struct {
    spinlock_t lock;          // Protects the rwlock itself
    volatile int32_t readers; // Number of active readers
    volatile bool writer;     // Is there an active writer?
    volatile int32_t waiting_writers; // Number of writers waiting
} rwlock_t;

#define RWLOCK_INIT { SPINLOCK_INIT, 0, false, 0 }

/**
 * Initialize a read-write lock
 */
void rwlock_init(rwlock_t* rwlock);

/**
 * Acquire read lock (multiple readers allowed)
 */
void rwlock_read_lock(rwlock_t* rwlock);

/**
 * Release read lock
 */
void rwlock_read_unlock(rwlock_t* rwlock);

/**
 * Acquire write lock (exclusive)
 */
void rwlock_write_lock(rwlock_t* rwlock);

/**
 * Release write lock
 */
void rwlock_write_unlock(rwlock_t* rwlock);

#endif // KERNEL_SYNC_RWLOCK_H

