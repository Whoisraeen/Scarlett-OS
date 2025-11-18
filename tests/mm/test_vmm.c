/**
 * @file test_vmm.c
 * @brief Unit tests for Virtual Memory Manager
 */

#include "../framework/test.h"
#include "../../kernel/include/types.h"
#include "../../kernel/include/mm/vmm.h"
#include "../../kernel/include/mm/pmm.h"
#include "../../kernel/include/kprintf.h"

/**
 * Test basic page mapping
 */
bool test_vmm_map_unmap(void) {
    kinfo("  Testing page mapping/unmapping...\n");

    address_space_t* as = vmm_get_kernel_address_space();
    TEST_ASSERT_NOT_NULL(as, "Kernel address space should exist");

    // Allocate a physical page
    paddr_t phys = pmm_alloc_page();
    TEST_ASSERT_NEQ(phys, 0, "Physical allocation should succeed");

    // Map it to a virtual address
    vaddr_t virt = 0xFFFFFFFF90000000ULL;  // Some kernel virtual address
    int result = vmm_map_page(as, virt, phys, VMM_PRESENT | VMM_WRITE);
    TEST_ASSERT_EQ(result, 0, "Mapping should succeed");

    // Get the physical address back
    paddr_t phys_back = vmm_get_physical(as, virt);
    TEST_ASSERT_EQ(phys_back, phys, "Physical address should match");

    // Unmap the page
    vmm_unmap_page(as, virt);

    // Clean up
    pmm_free_page(phys);

    return true;
}

/**
 * Test multiple page mapping
 */
bool test_vmm_map_multiple(void) {
    kinfo("  Testing multiple page mapping...\n");

    address_space_t* as = vmm_get_kernel_address_space();

    // Allocate physical pages
    paddr_t phys1 = pmm_alloc_page();
    paddr_t phys2 = pmm_alloc_page();
    paddr_t phys3 = pmm_alloc_page();

    TEST_ASSERT_NEQ(phys1, 0, "Allocation 1 should succeed");
    TEST_ASSERT_NEQ(phys2, 0, "Allocation 2 should succeed");
    TEST_ASSERT_NEQ(phys3, 0, "Allocation 3 should succeed");

    // Map them
    vaddr_t virt1 = 0xFFFFFFFF90000000ULL;
    vaddr_t virt2 = virt1 + PAGE_SIZE;
    vaddr_t virt3 = virt2 + PAGE_SIZE;

    TEST_ASSERT_EQ(vmm_map_page(as, virt1, phys1, VMM_PRESENT | VMM_WRITE), 0, "Map 1");
    TEST_ASSERT_EQ(vmm_map_page(as, virt2, phys2, VMM_PRESENT | VMM_WRITE), 0, "Map 2");
    TEST_ASSERT_EQ(vmm_map_page(as, virt3, phys3, VMM_PRESENT | VMM_WRITE), 0, "Map 3");

    // Verify mappings
    TEST_ASSERT_EQ(vmm_get_physical(as, virt1), phys1, "Mapping 1 correct");
    TEST_ASSERT_EQ(vmm_get_physical(as, virt2), phys2, "Mapping 2 correct");
    TEST_ASSERT_EQ(vmm_get_physical(as, virt3), phys3, "Mapping 3 correct");

    // Unmap
    vmm_unmap_page(as, virt1);
    vmm_unmap_page(as, virt2);
    vmm_unmap_page(as, virt3);

    // Clean up
    pmm_free_page(phys1);
    pmm_free_page(phys2);
    pmm_free_page(phys3);

    return true;
}

/**
 * Test unmapped page query
 */
bool test_vmm_unmapped(void) {
    kinfo("  Testing unmapped page query...\n");

    address_space_t* as = vmm_get_kernel_address_space();

    // Query unmapped address
    vaddr_t unmapped = 0xFFFFFFFF99999000ULL;
    paddr_t phys = vmm_get_physical(as, unmapped);

    TEST_ASSERT_EQ(phys, 0, "Unmapped address should return 0");

    return true;
}

/**
 * Test address space creation
 */
bool test_vmm_create_address_space(void) {
    kinfo("  Testing address space creation...\n");

    // Create new address space
    address_space_t* as = vmm_create_address_space();
    TEST_ASSERT_NOT_NULL(as, "Address space creation should succeed");
    TEST_ASSERT_NOT_NULL(as->pml4, "PML4 should be allocated");
    TEST_ASSERT_NEQ(as->asid, 0, "ASID should be assigned");

    // Destroy it
    vmm_destroy_address_space(as);

    return true;
}

/**
 * Run all VMM tests
 */
void run_vmm_tests(void) {
    kinfo("\n=== Virtual Memory Manager Tests ===\n");

    RUN_TEST(test_vmm_map_unmap);
    RUN_TEST(test_vmm_map_multiple);
    RUN_TEST(test_vmm_unmapped);
    RUN_TEST(test_vmm_create_address_space);

    kinfo("=== VMM Tests Complete ===\n\n");
}
