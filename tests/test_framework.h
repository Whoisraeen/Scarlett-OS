/**
 * @file test_framework.h
 * @brief ScarlettOS Test Framework
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Test result tracking
typedef struct {
    uint32_t total;
    uint32_t passed;
    uint32_t failed;
    uint32_t skipped;
} test_results_t;

// Global test results
extern test_results_t g_test_results;

// Test macros
#define TEST(name) \
    static void test_##name(void); \
    static void __attribute__((constructor)) register_##name(void) { \
        printf("Registering test: %s\n", #name); \
    } \
    static void test_##name(void)

#define ASSERT(condition) \
    do { \
        g_test_results.total++; \
        if (!(condition)) { \
            printf("  ✗ FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            g_test_results.failed++; \
            return; \
        } else { \
            g_test_results.passed++; \
        } \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        g_test_results.total++; \
        if ((a) != (b)) { \
            printf("  ✗ FAIL: %s:%d: %s == %s (%lld != %lld)\n", \
                   __FILE__, __LINE__, #a, #b, (long long)(a), (long long)(b)); \
            g_test_results.failed++; \
            return; \
        } else { \
            g_test_results.passed++; \
        } \
    } while(0)

#define ASSERT_NE(a, b) \
    do { \
        g_test_results.total++; \
        if ((a) == (b)) { \
            printf("  ✗ FAIL: %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
            g_test_results.failed++; \
            return; \
        } else { \
            g_test_results.passed++; \
        } \
    } while(0)

#define ASSERT_NULL(ptr) ASSERT_EQ(ptr, NULL)
#define ASSERT_NOT_NULL(ptr) ASSERT_NE(ptr, NULL)

#define RUN_TEST(name) \
    do { \
        printf("Running test: %s\n", #name); \
        test_##name(); \
    } while(0)

// Test framework functions
void test_init(void);
void test_print_results(void);
int test_get_exit_code(void);

#endif // TEST_FRAMEWORK_H
