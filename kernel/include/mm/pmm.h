/**
 * @file pmm.h
 * @brief Physical Memory Manager interface
 */

#ifndef KERNEL_MM_PMM_H
#define KERNEL_MM_PMM_H

#include "../types.h"
#include "../../../bootloader/common/boot_info.h"

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

/**
 * Initialize physical memory manager
 * @param boot_info Boot information containing memory map
 */
void pmm_init(boot_info_t* boot_info);

/**
 * Allocate a single physical page (4KB)
 * @return Physical address of allocated page, or 0 if out of memory
 */
paddr_t pmm_alloc_page(void);

/**
 * Allocate a single physical page in low memory (< 128MB)
 * @return Physical address of allocated page, or 0 if out of memory
 */
paddr_t pmm_alloc_page_low(void);

/**
 * Free a single physical page
 * @param page Physical address of page to free
 */
void pmm_free_page(paddr_t page);

/**
 * Allocate multiple contiguous physical pages
 * @param count Number of pages to allocate
 * @return Physical address of first page, or 0 if out of memory
 */
paddr_t pmm_alloc_pages(size_t count);

/**
 * Free multiple contiguous physical pages
 * @param base Physical address of first page
 * @param count Number of pages to free
 */
void pmm_free_pages(paddr_t base, size_t count);

/**
 * Get number of free pages
 * @return Number of free pages
 */
size_t pmm_get_free_pages(void);

/**
 * Get total number of pages
 * @return Total number of pages
 */
size_t pmm_get_total_pages(void);

/**
 * Increment reference count for a page (for Copy-on-Write)
 * @param page Physical address of page
 */
void pmm_ref_page(paddr_t page);

/**
 * Get reference count for a page
 * @param page Physical address of page
 * @return Reference count
 */
uint16_t pmm_get_refcount(paddr_t page);

/**
 * Convert physical address to page frame number
 */
#define PADDR_TO_PFN(addr) ((addr) >> PAGE_SHIFT)

/**
 * Convert page frame number to physical address
 */
#define PFN_TO_PADDR(pfn) ((pfn) << PAGE_SHIFT)

#endif // KERNEL_MM_PMM_H

