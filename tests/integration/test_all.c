/**
 * @file test_all.c
 * @brief Run all integration tests
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kprintf.h"

extern void run_service_tests(void);
extern void run_filesystem_tests(void);
extern void run_network_tests(void);

/**
 * Main test entry point
 */
void test_all(void) {
    kinfo("========================================\n");
    kinfo("  OS Integration Test Suite\n");
    kinfo("========================================\n\n");
    
    run_service_tests();
    run_filesystem_tests();
    run_network_tests();
    
    kinfo("\n========================================\n");
    kinfo("  All Tests Complete\n");
    kinfo("========================================\n");
}

