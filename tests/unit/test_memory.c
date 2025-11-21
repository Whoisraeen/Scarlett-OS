/**
 * @file test_memory.c
 * @brief Memory Management Unit Tests
 */

#include "../test_framework.h"

// Mock memory functions for testing
extern void* kmalloc(size_t size);
extern void kfree(void* ptr);
extern void* page_alloc(uint32_t order);
extern void page_free(void* page, uint32_t order);

TEST(memory_alloc_free) {
    void* ptr = kmalloc(1024);
    ASSERT_NOT_NULL(ptr);
    kfree(ptr);
}

TEST(memory_alloc_zero) {
    void* ptr = kmalloc(0);
    ASSERT_NULL(ptr);
}

TEST(memory_page_alloc) {
    void* page = page_alloc(0);
    ASSERT_NOT_NULL(page);
    page_free(page, 0);
}

TEST(memory_multiple_allocs) {
    void* ptrs[10];
    
    for (int i = 0; i < 10; i++) {
        ptrs[i] = kmalloc(128);
        ASSERT_NOT_NULL(ptrs[i]);
    }
    
    for (int i = 0; i < 10; i++) {
        kfree(ptrs[i]);
    }
}

int main(void) {
    test_init();
    
    printf("=== Memory Management Tests ===\n");
    RUN_TEST(memory_alloc_free);
    RUN_TEST(memory_alloc_zero);
    RUN_TEST(memory_page_alloc);
    RUN_TEST(memory_multiple_allocs);
    
    test_print_results();
    return test_get_exit_code();
}
