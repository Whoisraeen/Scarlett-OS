/**
 * @file mutex.c
 * @brief Mutex implementation
 */

#include "../include/types.h"
#include "../include/sync/mutex.h"
#include "../include/sched/scheduler.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

/**
 * Initialize mutex
 */
void mutex_init(mutex_t* mutex) {
    if (!mutex) {
        return;
    }
    
    spinlock_init(&mutex->lock);
    mutex->locked = false;
    mutex->owner_tid = 0;
    mutex->wait_count = 0;
}

/**
 * Lock mutex
 */
void mutex_lock(mutex_t* mutex) {
    if (!mutex) {
        return;
    }
    
    extern thread_t* thread_current(void);
    thread_t* current = thread_current();
    uint32_t tid = current ? current->tid : 0;
    
    while (1) {
        spinlock_lock(&mutex->lock);
        
        if (!mutex->locked) {
            // Got the lock
            mutex->locked = true;
            mutex->owner_tid = tid;
            spinlock_unlock(&mutex->lock);
            return;
        }
        
        // Lock is held, wait
        mutex->wait_count++;
        spinlock_unlock(&mutex->lock);
        
        // Yield CPU (in a real OS, we'd block the thread)
        extern void thread_yield(void);
        thread_yield();
        
        // Decrement wait count when we wake up
        spinlock_lock(&mutex->lock);
        mutex->wait_count--;
        spinlock_unlock(&mutex->lock);
    }
}

/**
 * Unlock mutex
 */
void mutex_unlock(mutex_t* mutex) {
    if (!mutex) {
        return;
    }
    
    extern thread_t* thread_current(void);
    thread_t* current = thread_current();
    uint32_t tid = current ? current->tid : 0;
    
    spinlock_lock(&mutex->lock);
    
    if (!mutex->locked) {
        spinlock_unlock(&mutex->lock);
        return;  // Not locked
    }
    
    if (mutex->owner_tid != tid) {
        spinlock_unlock(&mutex->lock);
        kerror("Mutex: Attempt to unlock mutex not owned by current thread\n");
        return;
    }
    
    mutex->locked = false;
    mutex->owner_tid = 0;
    
    spinlock_unlock(&mutex->lock);
}

/**
 * Try to lock mutex (non-blocking)
 */
bool mutex_trylock(mutex_t* mutex) {
    if (!mutex) {
        return false;
    }
    
    extern thread_t* thread_current(void);
    thread_t* current = thread_current();
    uint32_t tid = current ? current->tid : 0;
    
    spinlock_lock(&mutex->lock);
    
    if (!mutex->locked) {
        mutex->locked = true;
        mutex->owner_tid = tid;
        spinlock_unlock(&mutex->lock);
        return true;
    }
    
    spinlock_unlock(&mutex->lock);
    return false;
}

/**
 * Check if mutex is locked
 */
bool mutex_is_locked(mutex_t* mutex) {
    if (!mutex) {
        return false;
    }
    
    spinlock_lock(&mutex->lock);
    bool locked = mutex->locked;
    spinlock_unlock(&mutex->lock);
    
    return locked;
}

