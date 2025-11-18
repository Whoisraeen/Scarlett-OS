/**
 * @file paging.c
 * @brief Page table setup implementation
 */

#include "paging.h"

// Page table structures
typedef uint64_t pml4_entry_t;
typedef uint64_t pdp_entry_t;
typedef uint64_t pd_entry_t;
typedef uint64_t pt_entry_t;

/**
 * Allocate a page table
 */
static EFI_STATUS alloc_page_table(uint64_t** table, EFI_BOOT_SERVICES* bs) {
    EFI_PHYSICAL_ADDRESS addr = 0;
    EFI_STATUS status = bs->AllocatePages(
        0, // AllocateAnyPages
        EfiLoaderData,
        1, // 1 page = 4KB
        &addr
    );
    
    if (status != EFI_SUCCESS) {
        return status;
    }
    
    *table = (uint64_t*)addr;
    
    // Clear table
    for (int i = 0; i < 512; i++) {
        (*table)[i] = 0;
    }
    
    return EFI_SUCCESS;
}

/**
 * Map a page in the page tables
 */
static EFI_STATUS map_page(uint64_t* pml4, uint64_t virt_addr, uint64_t phys_addr,
                           uint64_t flags, EFI_BOOT_SERVICES* bs) {
    // Extract indices
    uint64_t pml4_idx = (virt_addr >> 39) & 0x1FF;
    uint64_t pdp_idx = (virt_addr >> 30) & 0x1FF;
    uint64_t pd_idx = (virt_addr >> 21) & 0x1FF;
    uint64_t pt_idx = (virt_addr >> 12) & 0x1FF;
    
    // Get or create PDP
    pdp_entry_t* pdp;
    if (!(pml4[pml4_idx] & PAGE_PRESENT)) {
        EFI_STATUS status = alloc_page_table((uint64_t**)&pdp, bs);
        if (status != EFI_SUCCESS) return status;
        pml4[pml4_idx] = (uint64_t)pdp | PAGE_PRESENT | PAGE_WRITE;
    } else {
        pdp = (pdp_entry_t*)(pml4[pml4_idx] & 0xFFFFFFFFF000);
    }
    
    // Get or create PD
    pd_entry_t* pd;
    if (!(pdp[pdp_idx] & PAGE_PRESENT)) {
        EFI_STATUS status = alloc_page_table((uint64_t**)&pd, bs);
        if (status != EFI_SUCCESS) return status;
        pdp[pdp_idx] = (uint64_t)pd | PAGE_PRESENT | PAGE_WRITE;
    } else {
        pd = (pd_entry_t*)(pdp[pdp_idx] & 0xFFFFFFFFF000);
    }
    
    // Get or create PT
    pt_entry_t* pt;
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        EFI_STATUS status = alloc_page_table((uint64_t**)&pt, bs);
        if (status != EFI_SUCCESS) return status;
        pd[pd_idx] = (uint64_t)pt | PAGE_PRESENT | PAGE_WRITE;
    } else {
        pt = (pt_entry_t*)(pd[pd_idx] & 0xFFFFFFFFF000);
    }
    
    // Set page table entry
    pt[pt_idx] = (phys_addr & 0xFFFFFFFFF000) | flags;
    
    return EFI_SUCCESS;
}

/**
 * Set up page tables
 */
EFI_STATUS setup_page_tables(uint64_t* pml4_addr,
                              uint64_t kernel_phys_start,
                              uint64_t kernel_phys_end,
                              uint64_t framebuffer_addr,
                              uint64_t framebuffer_size,
                              EFI_BOOT_SERVICES* bs) {
    // Allocate PML4
    uint64_t* pml4;
    EFI_STATUS status = alloc_page_table(&pml4, bs);
    if (status != EFI_SUCCESS) {
        return status;
    }
    
    *pml4_addr = (uint64_t)pml4;
    
    // Identity map first 1GB (for bootloader)
    for (uint64_t addr = 0; addr < 0x40000000; addr += 0x1000) {
        status = map_page(pml4, addr, addr, 
                         PAGE_PRESENT | PAGE_WRITE, bs);
        if (status != EFI_SUCCESS) return status;
    }
    
    // Map kernel to higher half (0xFFFFFFFF80000000)
    uint64_t kernel_virt_base = 0xFFFFFFFF80000000ULL;
    kernel_phys_start &= ~0xFFF;  // Align down
    kernel_phys_end = (kernel_phys_end + 0xFFF) & ~0xFFF;  // Align up
    
    for (uint64_t offset = 0; offset < (kernel_phys_end - kernel_phys_start); offset += 0x1000) {
        uint64_t virt = kernel_virt_base + offset;
        uint64_t phys = kernel_phys_start + offset;
        
        status = map_page(pml4, virt, phys,
                         PAGE_PRESENT | PAGE_WRITE, bs);
        if (status != EFI_SUCCESS) return status;
    }
    
    // Map framebuffer if present
    if (framebuffer_addr != 0) {
        framebuffer_addr &= ~0xFFF;
        framebuffer_size = (framebuffer_size + 0xFFF) & ~0xFFF;
        
        for (uint64_t offset = 0; offset < framebuffer_size; offset += 0x1000) {
            status = map_page(pml4, framebuffer_addr + offset, 
                            framebuffer_addr + offset,
                            PAGE_PRESENT | PAGE_WRITE | PAGE_NOCACHE, bs);
            if (status != EFI_SUCCESS) return status;
        }
    }
    
    // Map physical memory direct map (0xFFFF800000000000)
    // Map first 4GB for now
    uint64_t phys_map_base = 0xFFFF800000000000ULL;
    for (uint64_t addr = 0; addr < 0x100000000ULL; addr += 0x200000) {
        // Use 2MB pages for direct map (set huge page bit)
        uint64_t virt = phys_map_base + addr;
        status = map_page(pml4, virt, addr,
                         PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE, bs);
        if (status != EFI_SUCCESS) return status;
    }
    
    return EFI_SUCCESS;
}

