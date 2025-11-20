/**
 * @file test_scheduler.c
 * @brief Test scheduler initialization and thread creation
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/sched/scheduler.h"
#include "../../kernel/include/kprintf.h"
#include "../../kernel/include/debug.h"

/**
 * Test thread function
 */
static void test_thread_func(void* arg) {
    uint64_t thread_id = (uint64_t)arg;
    kinfo("[TEST] Thread %lu running\n", thread_id);
    
    // Do some work
    for (int i = 0; i < 5; i++) {
        kinfo("[TEST] Thread %lu: iteration %d\n", thread_id, i);
        thread_yield();
    }
    
    kinfo("[TEST] Thread %lu exiting\n", thread_id);
    thread_exit();
}

/**
 * Test scheduler initialization
 */
void test_scheduler_init(void) {
    kinfo("=== Testing Scheduler Initialization ===\n");
    
    // Initialize scheduler
    scheduler_init();
    kinfo("[PASS] Scheduler initialized\n");
    
    // Test thread creation
    kinfo("Creating test threads...\n");
    uint64_t tid1 = thread_create(test_thread_func, (void*)1, THREAD_PRIORITY_NORMAL, "test_thread_1");
    uint64_t tid2 = thread_create(test_thread_func, (void*)2, THREAD_PRIORITY_NORMAL, "test_thread_2");
    
    if (tid1 == 0 || tid2 == 0) {
        kerror("[FAIL] Thread creation failed\n");
        return;
    }
    
    kinfo("[PASS] Threads created: %lu, %lu\n", tid1, tid2);
    
    // Yield to let threads run
    for (int i = 0; i < 10; i++) {
        thread_yield();
    }
    
    kinfo("[PASS] Scheduler test complete\n");
}

