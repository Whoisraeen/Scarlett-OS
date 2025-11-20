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
    
    // Find highest physical address in CONVENTIONAL memory only
    // Ignore device memory regions (framebuffer, etc.) for total_pages calculation
    for (uint32_t i = 0; i < boot_info->memory_map_count; i++) {
        memory_region_t* region = &boot_info->memory_map[i];
        if (region->type == MEMORY_TYPE_CONVENTIONAL) {
            paddr_t end = region->base + region->length;
            if (end > highest_addr) {
                highest_addr = end;
            }
        }
    }

    // Calculate total pages based on conventional memory
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
    
    // Mark first 2MB as used (for BIOS, bootloader page tables, etc.)
    // Bootloader typically allocates page tables starting around 0x100000
    // This prevents allocating pages that are already in use
    pmm_mark_used(0, 512);  // First 2MB reserved
    
    // Reserve some pages at the end of first 128MB for page table allocations
    // This ensures page tables are always accessible via PHYS_MAP_BASE
    // Reserve last 1MB of first 128MB (pages from 0x7F00000 to 0x8000000)
    // Actually, don't reserve - just ensure PMM prefers this range via alloc logic
    
    kinfo("PMM initialized: %lu MB total, %lu MB free, %lu MB used\n",
          (total_pages * PAGE_SIZE) / (1024 * 1024),
          (free_pages * PAGE_SIZE) / (1024 * 1024),
          (used_pages * PAGE_SIZE) / (1024 * 1024));
}

/**
 * Allocate a single physical page
 * Prefers pages in the first 128MB for better PHYS_MAP_BASE compatibility
 */
paddr_t pmm_alloc_page(void) {
    static pfn_t last_allocated = 0;  // Hint for next allocation
    
    // All memory is now accessible via PHYS_MAP_BASE (using 2MB huge pages)
    // Search from 2MB onwards to avoid reserved low memory
    pfn_t start_pfn = (2 * 1024 * 1024) / PAGE_SIZE;  // Start from 2MB

    // Search from start_pfn to end of memory
    for (pfn_t pfn = start_pfn; pfn < total_pages; pfn++) {
        if (!bitmap_test(pfn)) {
            bitmap_set(pfn);
            free_pages--;
            used_pages++;
            last_allocated = pfn + 1;
            return PFN_TO_PADDR(pfn);
        }
    }

    // If nothing from 2MB onwards, try 1MB to 2MB range
    for (pfn_t pfn = (1 * 1024 * 1024) / PAGE_SIZE; pfn < start_pfn; pfn++) {
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
 * Allocate a single physical page in low memory (< 128MB)
 * Used for page tables during VMM initialization
 */
paddr_t pmm_alloc_page_low(void) {
    // Search from 2MB to 128MB
    pfn_t start_pfn = (2 * 1024 * 1024) / PAGE_SIZE;
    pfn_t end_pfn = (128 * 1024 * 1024) / PAGE_SIZE;
    
    if (end_pfn > total_pages) {
        end_pfn = total_pages;
    }

    for (pfn_t pfn = start_pfn; pfn < end_pfn; pfn++) {
        if (!bitmap_test(pfn)) {
            bitmap_set(pfn);
            free_pages--;
            used_pages++;
            return PFN_TO_PADDR(pfn);
        }
    }
    
    // Try 1MB to 2MB as fallback
    for (pfn_t pfn = (1 * 1024 * 1024) / PAGE_SIZE; pfn < start_pfn; pfn++) {
        if (!bitmap_test(pfn)) {
            bitmap_set(pfn);
            free_pages--;
            used_pages++;
            return PFN_TO_PADDR(pfn);
        }
    }

    kerror("PMM: Out of low memory (< 128MB)!\n");
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

