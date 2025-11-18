/**
 * @file mutex.h
 * @brief Mutex synchronization primitives
 */

#ifndef KERNEL_SYNC_MUTEX_H
#define KERNEL_SYNC_MUTEX_H

#include "../types.h"
#include "spinlock.h"

// Mutex structure
typedef struct {
    spinlock_t lock;            // Internal spinlock
    volatile bool locked;       // Is mutex locked?
    uint32_t owner_tid;         // Thread ID of owner
    uint32_t wait_count;        // Number of waiting threads
} mutex_t;

// Mutex functions
void mutex_init(mutex_t* mutex);
void mutex_lock(mutex_t* mutex);
void mutex_unlock(mutex_t* mutex);
bool mutex_trylock(mutex_t* mutex);
bool mutex_is_locked(mutex_t* mutex);

#endif // KERNEL_SYNC_MUTEX_H

