/**
 * @file test_framework.c
 * @brief Test Framework Implementation
 */

#include "test_framework.h"
#include <stdio.h>

test_results_t g_test_results = {0};

void test_init(void) {
    g_test_results.total = 0;
    g_test_results.passed = 0;
    g_test_results.failed = 0;
    g_test_results.skipped = 0;
}

void test_print_results(void) {
    printf("\n");
    printf("=====================================\n");
    printf("Test Results:\n");
    printf("  Total:   %u\n", g_test_results.total);
    printf("  Passed:  %u ✓\n", g_test_results.passed);
    printf("  Failed:  %u ✗\n", g_test_results.failed);
    printf("  Skipped: %u\n", g_test_results.skipped);
    printf("=====================================\n");
    
    if (g_test_results.failed == 0) {
        printf("All tests passed!\n");
    } else {
        printf("%u test(s) failed!\n", g_test_results.failed);
    }
}

int test_get_exit_code(void) {
    return (g_test_results.failed == 0) ? 0 : 1;
}
