/**
 * @file test.c
 * @brief Testing framework implementation
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kprintf.h"
#include "../../kernel/include/debug.h"
#include "test.h"

// Global test results
test_results_t g_test_results = {0};

/**
 * Initialize test framework
 */
void test_init(void) {
    g_test_results.total_tests = 0;
    g_test_results.passed_tests = 0;
    g_test_results.failed_tests = 0;
    g_test_results.current_test_name = NULL;

    kinfo("\n");
    kinfo("====================================================\n");
    kinfo("           Scarlett OS Test Suite\n");
    kinfo("====================================================\n");
    kinfo("\n");
}

/**
 * Report test failure
 */
void test_fail(const char* file, int line, const char* msg) {
    kerror("  Assertion failed at %s:%d\n", file, line);
    kerror("  Message: %s\n", msg);
}

/**
 * Print test summary
 */
void test_summary(void) {
    kinfo("\n");
    kinfo("====================================================\n");
    kinfo("                 Test Summary\n");
    kinfo("====================================================\n");
    kinfo("Total tests:  %d\n", g_test_results.total_tests);
    kinfo("Passed:       %d\n", g_test_results.passed_tests);
    kinfo("Failed:       %d\n", g_test_results.failed_tests);
    kinfo("Success rate: %d%%\n",
          g_test_results.total_tests > 0
          ? (g_test_results.passed_tests * 100) / g_test_results.total_tests
          : 0);
    kinfo("====================================================\n");

    if (g_test_results.failed_tests == 0) {
        kinfo("✓ ALL TESTS PASSED!\n");
    } else {
        kerror("✗ %d TESTS FAILED!\n", g_test_results.failed_tests);
    }
    kinfo("\n");
}
