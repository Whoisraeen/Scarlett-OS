/**
 * @file test_pmm.c
 * @brief Unit tests for Physical Memory Manager
 */

#include "../framework/test.h"
#include "../../kernel/include/types.h"
#include "../../kernel/include/mm/pmm.h"
#include "../../kernel/include/kprintf.h"

/**
 * Test basic page allocation and freeing
 */
bool test_pmm_alloc_free(void) {
    kinfo("  Testing basic allocation/free...\n");

    // Allocate a page
    paddr_t page = pmm_alloc_page();
    TEST_ASSERT_NEQ(page, 0, "Page allocation should succeed");
    TEST_ASSERT(IS_ALIGNED(page, PAGE_SIZE), "Page should be aligned");

    // Free the page
    pmm_free_page(page);

    // Should be able to allocate again
    paddr_t page2 = pmm_alloc_page();
    TEST_ASSERT_NEQ(page2, 0, "Second allocation should succeed");

    // Clean up
    pmm_free_page(page2);

    return true;
}

/**
 * Test contiguous page allocation
 */
bool test_pmm_contiguous(void) {
    kinfo("  Testing contiguous allocation...\n");

    // Allocate 4 contiguous pages
    paddr_t base = pmm_alloc_pages(4);
    TEST_ASSERT_NEQ(base, 0, "Contiguous allocation should succeed");
    TEST_ASSERT(IS_ALIGNED(base, PAGE_SIZE), "Base should be aligned");

    // Verify pages are contiguous (we can't directly verify, but trust implementation)

    // Free the pages
    pmm_free_pages(base, 4);

    return true;
}

/**
 * Test double free detection
 */
bool test_pmm_double_free(void) {
    kinfo("  Testing double-free detection...\n");

    paddr_t page = pmm_alloc_page();
    TEST_ASSERT_NEQ(page, 0, "Allocation should succeed");

    // First free - should succeed
    pmm_free_page(page);

    // Second free - should warn but not crash
    pmm_free_page(page);

    // If we get here, double-free was handled
    return true;
}

/**
 * Test NULL page free
 */
bool test_pmm_null_free(void) {
    kinfo("  Testing NULL page free...\n");

    // Should handle gracefully
    pmm_free_page(0);

    return true;
}

/**
 * Test invalid page free
 */
bool test_pmm_invalid_free(void) {
    kinfo("  Testing invalid page free...\n");

    // Try to free unaligned address
    pmm_free_page(0x1234);  // Not page-aligned

    // Try to free out-of-range address
    pmm_free_page(0xFFFFFFFFFFFFFFFFULL);

    return true;
}

/**
 * Test memory statistics
 */
bool test_pmm_stats(void) {
    kinfo("  Testing memory statistics...\n");

    size_t total_before = pmm_get_total_pages();
    size_t free_before = pmm_get_free_pages();

    TEST_ASSERT_NEQ(total_before, 0, "Total pages should be non-zero");
    TEST_ASSERT_NEQ(free_before, 0, "Free pages should be non-zero");

    // Allocate some pages
    paddr_t page1 = pmm_alloc_page();
    paddr_t page2 = pmm_alloc_page();
    paddr_t page3 = pmm_alloc_page();

    size_t free_after_alloc = pmm_get_free_pages();
    TEST_ASSERT_EQ(free_after_alloc, free_before - 3, "Free pages should decrease by 3");

    // Free the pages
    pmm_free_page(page1);
    pmm_free_page(page2);
    pmm_free_page(page3);

    size_t free_after_free = pmm_get_free_pages();
    TEST_ASSERT_EQ(free_after_free, free_before, "Free pages should return to original");

    return true;
}

/**
 * Run all PMM tests
 */
void run_pmm_tests(void) {
    kinfo("\n=== Physical Memory Manager Tests ===\n");

    RUN_TEST(test_pmm_alloc_free);
    RUN_TEST(test_pmm_contiguous);
    RUN_TEST(test_pmm_double_free);
    RUN_TEST(test_pmm_null_free);
    RUN_TEST(test_pmm_invalid_free);
    RUN_TEST(test_pmm_stats);

    kinfo("=== PMM Tests Complete ===\n\n");
}
