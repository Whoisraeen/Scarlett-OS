/**
 * @file test_heap.c
 * @brief Unit tests for Kernel Heap Allocator
 */

#include "../framework/test.h"
#include "../../kernel/include/types.h"
#include "../../kernel/include/mm/heap.h"
#include "../../kernel/include/kprintf.h"

/**
 * Test basic allocation and free
 */
bool test_heap_alloc_free(void) {
    kinfo("  Testing basic allocation/free...\n");

    // Allocate small block
    void* ptr = kmalloc(64);
    TEST_ASSERT_NOT_NULL(ptr, "Small allocation should succeed");

    // Free it
    kfree(ptr);

    // Allocate larger block
    void* ptr2 = kmalloc(4096);
    TEST_ASSERT_NOT_NULL(ptr2, "Large allocation should succeed");

    kfree(ptr2);

    return true;
}

/**
 * Test zero-initialized allocation
 */
bool test_heap_zalloc(void) {
    kinfo("  Testing zero-initialized allocation...\n");

    size_t size = 256;
    uint8_t* ptr = (uint8_t*)kzalloc(size);
    TEST_ASSERT_NOT_NULL(ptr, "kzalloc should succeed");

    // Verify all bytes are zero
    for (size_t i = 0; i < size; i++) {
        TEST_ASSERT_EQ(ptr[i], 0, "All bytes should be zero");
    }

    kfree(ptr);

    return true;
}

/**
 * Test reallocation
 */
bool test_heap_realloc(void) {
    kinfo("  Testing reallocation...\n");

    // Allocate initial block
    void* ptr = kmalloc(128);
    TEST_ASSERT_NOT_NULL(ptr, "Initial allocation should succeed");

    // Write some data
    uint8_t* bytes = (uint8_t*)ptr;
    for (int i = 0; i < 128; i++) {
        bytes[i] = (uint8_t)i;
    }

    // Reallocate to larger size
    void* ptr2 = krealloc(ptr, 256);
    TEST_ASSERT_NOT_NULL(ptr2, "Reallocation should succeed");

    // Verify data preserved
    uint8_t* bytes2 = (uint8_t*)ptr2;
    for (int i = 0; i < 128; i++) {
        TEST_ASSERT_EQ(bytes2[i], (uint8_t)i, "Data should be preserved");
    }

    kfree(ptr2);

    return true;
}

/**
 * Test NULL pointer handling
 */
bool test_heap_null(void) {
    kinfo("  Testing NULL pointer handling...\n");

    // Free NULL should not crash
    kfree(NULL);

    // Realloc NULL should behave like malloc
    void* ptr = krealloc(NULL, 128);
    TEST_ASSERT_NOT_NULL(ptr, "krealloc(NULL, size) should allocate");

    kfree(ptr);

    return true;
}

/**
 * Test double free detection
 */
bool test_heap_double_free(void) {
    kinfo("  Testing double-free detection...\n");

    void* ptr = kmalloc(128);
    TEST_ASSERT_NOT_NULL(ptr, "Allocation should succeed");

    // First free - should succeed
    kfree(ptr);

    // Second free - should detect and warn
    kfree(ptr);  // Should print warning but not crash

    return true;
}

/**
 * Test multiple allocations
 */
bool test_heap_multiple(void) {
    kinfo("  Testing multiple allocations...\n");

    #define NUM_ALLOCS 10
    void* ptrs[NUM_ALLOCS];

    // Allocate multiple blocks
    for (int i = 0; i < NUM_ALLOCS; i++) {
        ptrs[i] = kmalloc(64 + i * 32);
        TEST_ASSERT_NOT_NULL(ptrs[i], "Allocation should succeed");
    }

    // Free them all
    for (int i = 0; i < NUM_ALLOCS; i++) {
        kfree(ptrs[i]);
    }

    return true;
}

/**
 * Test heap statistics
 */
bool test_heap_stats(void) {
    kinfo("  Testing heap statistics...\n");

    size_t total, used, free;

    heap_get_stats(&total, &used, &free);

    TEST_ASSERT_NEQ(total, 0, "Total size should be non-zero");
    TEST_ASSERT_EQ(total, used + free, "Total should equal used + free");

    // Allocate something
    void* ptr = kmalloc(1024);
    size_t used_after;
    heap_get_stats(&total, &used_after, &free);

    TEST_ASSERT(used_after > used, "Used should increase after allocation");

    kfree(ptr);

    return true;
}

/**
 * Test coalescing of free blocks
 */
bool test_heap_coalescing(void) {
    kinfo("  Testing free block coalescing...\n");

    // Allocate several adjacent blocks
    void* ptr1 = kmalloc(128);
    void* ptr2 = kmalloc(128);
    void* ptr3 = kmalloc(128);

    TEST_ASSERT_NOT_NULL(ptr1, "Alloc 1");
    TEST_ASSERT_NOT_NULL(ptr2, "Alloc 2");
    TEST_ASSERT_NOT_NULL(ptr3, "Alloc 3");

    // Free them - they should coalesce
    kfree(ptr1);
    kfree(ptr2);
    kfree(ptr3);

    // Should be able to allocate a large block that spans all three
    void* large = kmalloc(384);  // 128 * 3
    TEST_ASSERT_NOT_NULL(large, "Large allocation after coalescing should succeed");

    kfree(large);

    return true;
}

/**
 * Run all heap tests
 */
void run_heap_tests(void) {
    kinfo("\n=== Kernel Heap Allocator Tests ===\n");

    RUN_TEST(test_heap_alloc_free);
    RUN_TEST(test_heap_zalloc);
    RUN_TEST(test_heap_realloc);
    RUN_TEST(test_heap_null);
    RUN_TEST(test_heap_double_free);
    RUN_TEST(test_heap_multiple);
    RUN_TEST(test_heap_stats);
    RUN_TEST(test_heap_coalescing);

    kinfo("=== Heap Tests Complete ===\n\n");
}
