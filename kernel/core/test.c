/**
 * @file test.c
 * @brief Test functions for kernel features
 */

#include "../include/types.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/sched/scheduler.h"
#include "../include/string.h"

static int test_memcpy(void) {
    kprintf("[TEST] Running memcpy test...\n");
    char src[] = "Hello, world!";
    char dest[20];
    
    memcpy(dest, src, strlen(src) + 1);
    
    for (size_t i = 0; i < strlen(src) + 1; i++) {
        if (dest[i] != src[i]) {
            kprintf("  [FAIL] memcpy failed at index %d\n", i);
            return 1;
        }
    }
    
    kprintf("  [PASS] memcpy test passed\n");
    return 0;
}

static int test_memset(void) {
    kprintf("[TEST] Running memset test...\n");
    char buf[10];
    
    memset(buf, 'A', 10);
    
    for (int i = 0; i < 10; i++) {
        if (buf[i] != 'A') {
            kprintf("  [FAIL] memset failed at index %d\n", i);
            return 1;
        }
    }
    
    kprintf("  [PASS] memset test passed\n");
    return 0;
}

/**
 * Test thread function
 */
void test_thread(void* arg) {
    (void)arg;
    
    kinfo("[TEST THREAD] Hello from test thread!\n");
    kinfo("[TEST THREAD] Thread ID: %lu\n", thread_current()->tid);
    kinfo("[TEST THREAD] Thread name: %s\n", thread_current()->name);
    
    // Do some work
    for (int i = 0; i < 5; i++) {
        kinfo("[TEST THREAD] Iteration %d\n", i);
        thread_yield();
    }
    
    kinfo("[TEST THREAD] Test thread exiting\n");
    thread_exit();
}

/**
 * Run all kernel tests
 */
void run_all_tests(void) {
    kprintf("\n===== Running Kernel Tests =====\n");
    
    int failed = 0;
    failed += test_memcpy();
    failed += test_memset();
    
    if (failed == 0) {
        kprintf("===== All tests passed! =====\n\n");
    } else {
        kprintf("===== %d test(s) failed! =====\n\n", failed);
    }
}