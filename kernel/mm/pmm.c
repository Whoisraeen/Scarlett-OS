/**
 * @file pmm.c
 * @brief Physical Memory Manager implementation
 *
 * This is a simple bitmap-based physical memory allocator.
 * Each bit represents one 4KB page frame.
 * 0 = free, 1 = allocated
 */

#include "../include/types.h"
#include "../include/config.h"
#include "../include/mm/pmm.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

// Bitmap for tracking page allocation
#define MAX_PAGES (16 * 1024 * 256) // Support up to 16GB of RAM
static uint8_t page_bitmap[MAX_PAGES / 8];

// Statistics
static size_t total_pages = 0;
static size_t free_pages = 0;
static size_t used_pages = 0;

// Highest physical address
static paddr_t highest_addr = 0;

/**
 * Set a bit in the bitmap
 */
static inline void bitmap_set(size_t bit) {
    page_bitmap[bit / 8] |= (1 << (bit % 8));
}

/**
 * Clear a bit in the bitmap
 */
static inline void bitmap_clear(size_t bit) {
    page_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

/**
 * Test a bit in the bitmap
 */
static inline bool bitmap_test(size_t bit) {
    return (page_bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

/**
 * Mark a range of pages as used
 * Fixed: Added overflow check
 */
static void pmm_mark_used(paddr_t base, size_t count) {
    pfn_t pfn = PADDR_TO_PFN(base);
    
    // Overflow check
    if (pfn + count < pfn) {
        kerror("PMM: Integer overflow in pmm_mark_used\n");
        return;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (pfn + i < total_pages) {
            if (!bitmap_test(pfn + i)) {
                bitmap_set(pfn + i);
                if (free_pages > 0) {
                    free_pages--;
                }
                used_pages++;
            }
        }
    }
}

/**
 * Mark a range of pages as free
 * Fixed: Added overflow check
 */
static void pmm_mark_free(paddr_t base, size_t count) {
    pfn_t pfn = PADDR_TO_PFN(base);
    
    // Overflow check
    if (pfn + count < pfn) {
        kerror("PMM: Integer overflow in pmm_mark_free\n");
        return;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (pfn + i < total_pages) {
            if (bitmap_test(pfn + i)) {
                bitmap_clear(pfn + i);
                free_pages++;
                if (used_pages > 0) {
                    used_pages--;
                }
            }
        }
    }
}

/**
 * Initialize physical memory manager
 */
void pmm_init(boot_info_t* boot_info) {
    kinfo("Initializing Physical Memory Manager...\n");
    
    // Clear bitmap
    for (size_t i = 0; i < sizeof(page_bitmap); i++) {
        page_bitmap[i] = 0xFF; // Mark all as used initially
    }
    
    // Find highest physical address
    for (uint32_t i = 0; i < boot_info->memory_map_count; i++) {
        memory_region_t* region = &boot_info->memory_map[i];
        paddr_t end = region->base + region->length;
        if (end > highest_addr) {
            highest_addr = end;
        }
    }
    
    // Calculate total pages
    total_pages = PADDR_TO_PFN(highest_addr);
    if (total_pages > MAX_PAGES) {
        total_pages = MAX_PAGES;
        kwarn("Physical memory exceeds maximum supported, limiting to %lu pages\n", total_pages);
    }
    
    free_pages = 0;
    used_pages = total_pages;
    
    // Mark usable regions as free
    for (uint32_t i = 0; i < boot_info->memory_map_count; i++) {
        memory_region_t* region = &boot_info->memory_map[i];
        
        if (region->type == MEMORY_TYPE_CONVENTIONAL) {
            // Mark as free
            paddr_t base = ALIGN_UP(region->base, PAGE_SIZE);
            paddr_t end = ALIGN_DOWN(region->base + region->length, PAGE_SIZE);
            
            if (end > base) {
                size_t pages = (end - base) / PAGE_SIZE;
                pmm_mark_free(base, pages);
            }
        }
    }
    
    // Mark kernel memory as used
    extern uint8_t _kernel_start[], _kernel_end[];
    paddr_t kernel_start = (paddr_t)_kernel_start - KERNEL_VMA_BASE;
    paddr_t kernel_end = (paddr_t)_kernel_end - KERNEL_VMA_BASE;
    size_t kernel_pages = (kernel_end - kernel_start + PAGE_SIZE - 1) / PAGE_SIZE;
    pmm_mark_used(kernel_start, kernel_pages);
    
    // Mark first 1MB as used (for BIOS, etc.)
    pmm_mark_used(0, 256);
    
    kinfo("PMM initialized: %lu MB total, %lu MB free, %lu MB used\n",
          (total_pages * PAGE_SIZE) / (1024 * 1024),
          (free_pages * PAGE_SIZE) / (1024 * 1024),
          (used_pages * PAGE_SIZE) / (1024 * 1024));
}

/**
 * Allocate a single physical page
 */
paddr_t pmm_alloc_page(void) {
    static pfn_t last_allocated = 0;  // Hint for next allocation

    // Try from last allocated position first (locality)
    for (pfn_t pfn = last_allocated; pfn < total_pages; pfn++) {
        if (!bitmap_test(pfn)) {
            bitmap_set(pfn);
            free_pages--;
            used_pages++;
            last_allocated = pfn + 1;
            return PFN_TO_PADDR(pfn);
        }
    }

    // Wrap around and search from beginning
    for (pfn_t pfn = 0; pfn < last_allocated; pfn++) {
        if (!bitmap_test(pfn)) {
            bitmap_set(pfn);
            free_pages--;
            used_pages++;
            last_allocated = pfn + 1;
            return PFN_TO_PADDR(pfn);
        }
    }

    kerror("PMM: Out of physical memory!\n");
    return 0;
}

/**
 * Free a single physical page
 */
void pmm_free_page(paddr_t page) {
    if (page == 0) {
        kwarn("PMM: Attempt to free NULL page\n");
        return;
    }
    
    if (!IS_ALIGNED(page, PAGE_SIZE)) {
        kerror("PMM: Attempt to free unaligned page 0x%lx\n", page);
        return;
    }
    
    pfn_t pfn = PADDR_TO_PFN(page);
    
    if (pfn >= total_pages) {
        kerror("PMM: Attempt to free invalid page 0x%lx\n", page);
        return;
    }
    
    if (!bitmap_test(pfn)) {
        kwarn("PMM: Double free of page 0x%lx\n", page);
        return;
    }
    
    bitmap_clear(pfn);
    free_pages++;
    used_pages--;
}

/**
 * Allocate multiple contiguous pages
 */
paddr_t pmm_alloc_pages(size_t count) {
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_page();
    
    // Find contiguous free pages
    pfn_t start = 0;
    size_t found = 0;
    
    for (pfn_t pfn = 0; pfn < total_pages; pfn++) {
        if (!bitmap_test(pfn)) {
            if (found == 0) {
                start = pfn;
            }
            found++;
            
            if (found == count) {
                // Found enough contiguous pages
                for (size_t i = 0; i < count; i++) {
                    bitmap_set(start + i);
                    free_pages--;
                    used_pages++;
                }
                return PFN_TO_PADDR(start);
            }
        } else {
            found = 0;
        }
    }
    
    kerror("PMM: Could not allocate %lu contiguous pages\n", count);
    return 0;
}

/**
 * Free multiple contiguous pages
 */
void pmm_free_pages(paddr_t base, size_t count) {
    for (size_t i = 0; i < count; i++) {
        pmm_free_page(base + (i * PAGE_SIZE));
    }
}

/**
 * Get number of free pages
 */
size_t pmm_get_free_pages(void) {
    return free_pages;
}

/**
 * Get total number of pages
 */
size_t pmm_get_total_pages(void) {
    return total_pages;
}

