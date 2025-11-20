/**
 * @file vmm.h
 * @brief Virtual Memory Manager interface
 */

#ifndef KERNEL_MM_VMM_H
#define KERNEL_MM_VMM_H

#include "../types.h"
#include "../../../bootloader/common/boot_info.h"

// Page table entry flags
#define VMM_PRESENT    (1ULL << 0)
#define VMM_WRITE      (1ULL << 1)
#define VMM_USER       (1ULL << 2)
#define VMM_WRITETHROUGH (1ULL << 3)
#define VMM_NOCACHE    (1ULL << 4)
#define VMM_ACCESSED   (1ULL << 5)
#define VMM_DIRTY      (1ULL << 6)
#define VMM_HUGE       (1ULL << 7)
#define VMM_GLOBAL     (1ULL << 8)
#define VMM_COW        (1ULL << 9)  // Copy-on-Write flag (software-defined, bit 9)
#define VMM_NX         (1ULL << 63)

// Virtual address space structure
typedef struct address_space {
    uint64_t* pml4;              // Page table root
    uint64_t asid;               // Address space ID
    struct address_space* next;  // Linked list
} address_space_t;

/**
 * Initialize VMM with kernel page tables
 */
void vmm_init(void);

/**
 * Create a new address space
 */
address_space_t* vmm_create_address_space(void);

/**
 * Destroy an address space
 */
void vmm_destroy_address_space(address_space_t* as);

/**
 * Switch to an address space
 */
void vmm_switch_address_space(address_space_t* as);

/**
 * Map a virtual page to a physical page
 */
int vmm_map_page(address_space_t* as, vaddr_t vaddr, paddr_t paddr, uint64_t flags);

/**
 * Unmap a virtual page
 */
int vmm_unmap_page(address_space_t* as, vaddr_t vaddr);

/**
 * Get physical address for virtual address
 */
paddr_t vmm_get_physical(address_space_t* as, vaddr_t vaddr);

/**
 * Map multiple contiguous pages
 */
int vmm_map_pages(address_space_t* as, vaddr_t vaddr, paddr_t paddr, size_t count, uint64_t flags);

/**
 * Unmap multiple contiguous pages
 */
int vmm_unmap_pages(address_space_t* as, vaddr_t vaddr, size_t count);

/**
 * Flush TLB for a single address
 */
void vmm_flush_tlb_single(vaddr_t vaddr);

/**
 * Flush entire TLB
 */
void vmm_flush_tlb_all(void);

/**
 * Get kernel address space
 */
address_space_t* vmm_get_kernel_address_space(void);

/**
 * Handle Copy-on-Write page fault
 * @param vaddr Virtual address that caused the fault
 * @return 0 on success, -1 on error
 */
int vmm_handle_cow_fault(vaddr_t vaddr);

/**
 * Mark a page as Copy-on-Write (remove write permission, set CoW flag)
 * @param as Address space
 * @param vaddr Virtual address
 * @return 0 on success, -1 on error
 */
int vmm_mark_cow(address_space_t* as, vaddr_t vaddr);

#endif // KERNEL_MM_VMM_H

