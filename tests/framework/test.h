/**
 * @file test.h
 * @brief Simple testing framework for Scarlett OS
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdbool.h>
#include <stddef.h>

// Test result tracking
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    const char* current_test_name;
} test_results_t;

// Global test results
extern test_results_t g_test_results;

/**
 * Test assertion macros
 */
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            test_fail(__FILE__, __LINE__, message); \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(a, b, message) \
    TEST_ASSERT((a) == (b), message)

#define TEST_ASSERT_NEQ(a, b, message) \
    TEST_ASSERT((a) != (b), message)

#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT((ptr) == NULL, message)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT((ptr) != NULL, message)

#define TEST_ASSERT_TRUE(cond, message) \
    TEST_ASSERT((cond) == true, message)

#define TEST_ASSERT_FALSE(cond, message) \
    TEST_ASSERT((cond) == false, message)

/**
 * Run a test function
 */
#define RUN_TEST(test_func) \
    do { \
        g_test_results.current_test_name = #test_func; \
        g_test_results.total_tests++; \
        kinfo("[TEST] Running: %s\n", #test_func); \
        if (test_func()) { \
            g_test_results.passed_tests++; \
            kinfo("[PASS] %s\n", #test_func); \
        } else { \
            g_test_results.failed_tests++; \
            kerror("[FAIL] %s\n", #test_func); \
        } \
    } while(0)

/**
 * Test reporting functions
 */
void test_init(void);
void test_fail(const char* file, int line, const char* msg);
void test_summary(void);

#endif // TEST_FRAMEWORK_H
