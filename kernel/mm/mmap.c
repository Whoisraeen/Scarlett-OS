/**
 * @file mmap.c
 * @brief Memory mapping implementation
 */

#include "../include/types.h"
#include "../include/mm/mmap.h"
#include "../include/mm/vmm.h"
#include "../include/mm/pmm.h"
#include "../include/mm/heap.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/config.h"

// Helper to convert physical to virtual for page table access (same as vmm.c)
#define PHYS_MAP_BASE 0xFFFF800000000000ULL
extern bool phys_map_ready;
static inline void* phys_to_virt_pt(paddr_t paddr) {
    if (phys_map_ready) {
        return (void*)(paddr + PHYS_MAP_BASE);
    } else {
        return (void*)paddr;  // Identity mapping during early boot
    }
}

// Memory mapping list per address space
// static memory_mapping_t* mapping_list = NULL; // REMOVED

// User space memory region (lower half)
#define USER_SPACE_START 0x0000000000400000ULL  // 4MB (above code)
#define USER_SPACE_END   0x00007FFFFFFFFFFFULL
#define USER_SPACE_SIZE  (USER_SPACE_END - USER_SPACE_START)

// Current allocation pointer (simple bump allocator) - NOTE: This should also be per-AS!
// static vaddr_t current_brk = USER_SPACE_START; // REMOVED - Using logic to find holes instead or AS-specific brk

/**
 * Initialize memory mapping system
 */
error_code_t mmap_init(void) {
    kinfo("Initializing memory mapping system...\n");
    // mapping_list = NULL;
    // current_brk = USER_SPACE_START;
    kinfo("Memory mapping system initialized\n");
    return ERR_OK;
}

/**
 * Find a memory mapping
 */
memory_mapping_t* mmap_find(address_space_t* as, vaddr_t addr) {
    if (!as) return NULL;
    
    for (memory_mapping_t* m = as->mappings; m != NULL; m = m->next) {
        if (addr >= m->start && addr < m->end) {
            return m;
        }
    }
    return NULL;
}

/**
 * Allocate memory mapping
 */
vaddr_t mmap_alloc(address_space_t* as, size_t size, uint64_t prot, uint64_t flags, int fd, uint64_t offset) {
    if (!as || size == 0) {
        return (vaddr_t)ERR_INVALID_ARG;
    }
    
    // Align size to page boundary
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    // For anonymous mappings, allocate pages
    if (flags & MAP_ANONYMOUS || fd < 0) {
        // Find a free region
        // Simple algorithm: Start from USER_SPACE_START and look for a gap large enough
        
        vaddr_t start = USER_SPACE_START;
        vaddr_t end = start + size;
        
        // Check if we have space (basic check)
        if (end > USER_SPACE_END) return (vaddr_t)ERR_OUT_OF_MEMORY;
        
        // Iterate through existing mappings to find a hole
        // This assumes mappings are not sorted (O(N^2) worst case for finding hole if we just bump? No, 
        // standard way: sort by address. But for now, we just check for overlap and bump 'start' past 'end' of overlap.)
        // Better approach: Keep list sorted or use tree.
        // For now, iterate and if overlap, restart scan from new start.
        
        bool collision;
        do {
            collision = false;
            if (start + size > USER_SPACE_END) return (vaddr_t)ERR_OUT_OF_MEMORY;
            end = start + size;
            
            for (memory_mapping_t* m = as->mappings; m != NULL; m = m->next) {
                if ((start >= m->start && start < m->end) ||
                    (end > m->start && end <= m->end) ||
                    (start <= m->start && end >= m->end)) {
                    // Overlap found, move start to end of conflicting mapping
                    start = m->end;
                    // Align start to page? m->end should be page aligned.
                    collision = true;
                    break; // Restart scan with new start
                }
            }
        } while (collision);
        
        // Allocate and map pages
        size_t num_pages = size / PAGE_SIZE;
        for (size_t i = 0; i < num_pages; i++) {
            paddr_t page = pmm_alloc_page();
            if (page == 0) {
                // Free already allocated pages
                for (size_t j = 0; j < i; j++) {
                    vaddr_t page_vaddr = start + (j * PAGE_SIZE);
                    paddr_t page_paddr = vmm_get_physical(as, page_vaddr);
                    if (page_paddr != 0) {
                        vmm_unmap_page(as, page_vaddr);
                        pmm_free_page(page_paddr);
                    }
                }
                kerror("MMAP: Out of physical memory\n");
                return (vaddr_t)ERR_OUT_OF_MEMORY;
            }
            
            // Set up page flags
            uint64_t page_flags = VMM_PRESENT | VMM_USER;
            if (prot & PROT_WRITE) page_flags |= VMM_WRITE;
            if (!(prot & PROT_EXEC)) page_flags |= VMM_NX;
            
            vaddr_t page_vaddr = start + (i * PAGE_SIZE);
            if (vmm_map_page(as, page_vaddr, page, page_flags) != 0) {
                pmm_free_page(page);
                // Free already allocated pages
                for (size_t j = 0; j < i; j++) {
                    vaddr_t free_vaddr = start + (j * PAGE_SIZE);
                    paddr_t free_paddr = vmm_get_physical(as, free_vaddr);
                    if (free_paddr != 0) {
                        vmm_unmap_page(as, free_vaddr);
                        pmm_free_page(free_paddr);
                    }
                }
                kerror("MMAP: Failed to map page\n");
                return (vaddr_t)ERR_MAPPING_FAILED;
            }
            
            // Clear the page
            uint8_t* page_data = (uint8_t*)(page + PHYS_MAP_BASE);
            memset(page_data, 0, PAGE_SIZE);
        }
        
        // Create mapping entry
        memory_mapping_t* mapping = (memory_mapping_t*)kmalloc(sizeof(memory_mapping_t));
        if (!mapping) {
            // Free pages
            // ... (cleanup logic similar to above)
             for (size_t i = 0; i < num_pages; i++) {
                vaddr_t page_vaddr = start + (i * PAGE_SIZE);
                paddr_t page_paddr = vmm_get_physical(as, page_vaddr);
                if (page_paddr != 0) {
                    vmm_unmap_page(as, page_vaddr);
                    pmm_free_page(page_paddr);
                }
            }
            return (vaddr_t)ERR_OUT_OF_MEMORY;
        }
        
        mapping->start = start;
        mapping->end = end;
        mapping->size = size;
        mapping->flags = prot | flags;
        mapping->fd = fd;
        mapping->offset = offset;
        
        // Add to address space list
        mapping->next = as->mappings;
        as->mappings = mapping;
        
        kinfo("MMAP: Allocated %lu bytes at 0x%016lx\n", size, start);
        return start;
    }
    
    // File-backed mappings not yet supported
    kerror("MMAP: File-backed mappings not yet supported\n");
    return (vaddr_t)ERR_NOT_SUPPORTED;
}

/**
 * Free memory mapping
 */
error_code_t mmap_free(address_space_t* as, vaddr_t addr, size_t size) {
    if (!as || addr == 0) {
        return ERR_INVALID_ARG;
    }
    
    // Align to page boundary
    vaddr_t start = addr & ~(PAGE_SIZE - 1);
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    vaddr_t end = start + size;
    
    // Find mapping
    memory_mapping_t* prev = NULL;
    memory_mapping_t* m = as->mappings;
    
    while (m) {
        if (m->start == start && m->end == end) {
            // Found exact match
            break;
        }
        // Logic for partial unmapping is complex (splitting nodes), simplified here to exact match only for now
        // or "start >= m->start && end <= m->end"
        
        if (start >= m->start && end <= m->end) {
             // Found containing mapping
             break;
        }
        
        prev = m;
        m = m->next;
    }
    
    if (!m) {
        kerror("MMAP: Mapping not found at 0x%016lx\n", addr);
        return ERR_INVALID_ADDRESS;
    }
    
    // Unmap pages
    size_t num_pages = size / PAGE_SIZE;
    for (size_t i = 0; i < num_pages; i++) {
        vaddr_t page_vaddr = start + (i * PAGE_SIZE);
        paddr_t page_paddr = vmm_get_physical(as, page_vaddr);
        if (page_paddr != 0) {
            vmm_unmap_page(as, page_vaddr);
            // Only free if anonymous mapping
            if (m->flags & MAP_ANONYMOUS || m->fd < 0) {
                pmm_free_page(page_paddr);
            }
        }
    }
    
    // If exact match, remove from list
    if (m->start == start && m->end == end) {
        if (prev) {
            prev->next = m->next;
        } else {
            as->mappings = m->next;
        }
        kfree(m);
    } else {
        // Partial unmap logic (TODO: Split mapping)
        // For now, we just keep the metadata but pages are unmapped.
        // This is "simplified" but safer than crashing.
    }
    
    kinfo("MMAP: Freed %lu bytes at 0x%016lx\n", size, start);
    return ERR_OK;
}

/**
 * Change protection of memory mapping
 */
error_code_t mmap_protect(address_space_t* as, vaddr_t addr, size_t size, uint64_t prot) {
    if (!as || addr == 0) {
        return ERR_INVALID_ARG;
    }
    
    // Find mapping
    memory_mapping_t* m = mmap_find(as, addr);
    if (!m) {
        return ERR_INVALID_ADDRESS;
    }
    
    // Update protection flags
    m->flags = (m->flags & ~0x07) | (prot & 0x07);
    
    // Update page table entries
    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < num_pages; i++) {
        vaddr_t page_vaddr = (addr & ~(PAGE_SIZE - 1)) + (i * PAGE_SIZE);
        paddr_t page_paddr = vmm_get_physical(as, page_vaddr);
        if (page_paddr == 0) {
            continue;
        }
        
        // Get current page table entry and update flags
        // Modify page table entry to update protection flags
        // Walk page tables to find the PTE
        uint64_t* table = as->pml4;
        int indices[4];
        indices[0] = (page_vaddr >> 39) & 0x1FF;  // PML4
        indices[1] = (page_vaddr >> 30) & 0x1FF;  // PDPT
        indices[2] = (page_vaddr >> 21) & 0x1FF;  // PD
        indices[3] = (page_vaddr >> 12) & 0x1FF;  // PT
        
        // Walk page tables
        for (int level = 0; level < 4; level++) {
            if (!table) break;
            uint64_t entry = table[indices[level]];
            if (!(entry & VMM_PRESENT)) break;
            
            if (level == 3) {
                // This is the page table entry
                uint64_t* pte = &table[indices[level]];
                
                // Get current flags
                uint64_t current_flags = *pte & 0xFFF;  // Lower 12 bits are flags
                
                // Update protection flags (bits 0-2: Present, Write, User)
                uint64_t new_flags = (current_flags & ~0x07ULL) | (prot & 0x07ULL);
                
                // Preserve other flags (bits 3-11)
                uint64_t preserved_flags = current_flags & 0xFF8ULL;
                
                // Combine: physical address + new protection + preserved flags
                uint64_t new_entry = (*pte & 0xFFFFFFFFF000ULL) | new_flags | preserved_flags;
                
                // Update page table entry atomically
                *pte = new_entry;
                
                // Flush TLB for this page
                __asm__ volatile("invlpg (%0)" :: "r"(page_vaddr) : "memory");
                break;
            } else {
                // Get next level table
                paddr_t next_table_phys = entry & 0xFFFFFFFFF000ULL;
                table = (uint64_t*)phys_to_virt_pt(next_table_phys);
            }
        }
    }
    
    return ERR_OK;
}

