/**
 * @file semaphore.h
 * @brief Semaphore synchronization primitives
 */

#ifndef KERNEL_SYNC_SEMAPHORE_H
#define KERNEL_SYNC_SEMAPHORE_H

#include "../types.h"
#include "spinlock.h"

// Semaphore structure
typedef struct {
    spinlock_t lock;            // Internal spinlock
    volatile uint32_t count;    // Semaphore count
    uint32_t max_count;          // Maximum count
    uint32_t wait_count;         // Number of waiting threads
} semaphore_t;

// Semaphore functions
void semaphore_init(semaphore_t* sem, uint32_t initial_count, uint32_t max_count);
void semaphore_wait(semaphore_t* sem);
void semaphore_signal(semaphore_t* sem);
bool semaphore_trywait(semaphore_t* sem);
uint32_t semaphore_get_count(semaphore_t* sem);

#endif // KERNEL_SYNC_SEMAPHORE_H

