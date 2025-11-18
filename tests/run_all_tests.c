/**
 * @file run_all_tests.c
 * @brief Main test runner for all kernel unit tests
 *
 * This file is compiled as part of the kernel and can be called
 * from kernel_main() to run all tests.
 */

#include "../kernel/include/types.h"
#include "../kernel/include/kprintf.h"
#include "../kernel/include/debug.h"
#include "framework/test.h"

// Declare test runners
extern void run_pmm_tests(void);
extern void run_vmm_tests(void);
extern void run_heap_tests(void);

/**
 * Run all kernel unit tests
 *
 * Call this from kernel_main() after all subsystems are initialized.
 */
void run_all_kernel_tests(void) {
    test_init();

    // Phase 1 tests
    kinfo("\n");
    kinfo("╔══════════════════════════════════════════════════╗\n");
    kinfo("║           PHASE 1: Memory Management            ║\n");
    kinfo("╚══════════════════════════════════════════════════╝\n");

    run_pmm_tests();
    run_vmm_tests();
    run_heap_tests();

    // Phase 2 tests (when implemented)
    // run_scheduler_tests();
    // run_ipc_tests();
    // run_syscall_tests();

    test_summary();
}

/**
 * Quick smoke test - just verify basics work
 *
 * Call this early in boot to catch critical failures.
 */
void run_smoke_tests(void) {
    kinfo("\n=== Running Smoke Tests ===\n");

    // Test serial output
    kinfo("  ✓ Serial output working\n");

    // Test PMM
    paddr_t test_page = pmm_alloc_page();
    if (test_page != 0) {
        kinfo("  ✓ PMM allocation working\n");
        pmm_free_page(test_page);
        kinfo("  ✓ PMM free working\n");
    } else {
        kerror("  ✗ PMM allocation FAILED\n");
    }

    // Test heap (if initialized)
    extern bool heap_is_initialized(void);  // You'll need to add this function
    if (heap_is_initialized()) {
        void* test_ptr = kmalloc(64);
        if (test_ptr) {
            kinfo("  ✓ Heap allocation working\n");
            kfree(test_ptr);
            kinfo("  ✓ Heap free working\n");
        } else {
            kerror("  ✗ Heap allocation FAILED\n");
        }
    }

    kinfo("=== Smoke Tests Complete ===\n\n");
}
