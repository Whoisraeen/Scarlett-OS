/**
 * @file vmm.c
 * @brief Virtual Memory Manager implementation
 */

#include "../include/types.h"
#include "../include/config.h"
#include "../include/mm/vmm.h"
#include "../include/mm/pmm.h"
#include "../include/mm/bootstrap.h"
#include "../include/mm/heap.h"
#include "../include/process.h"
#include "../include/string.h"
#include "../include/kprintf.h"
#include "../include/debug.h"


// Kernel address space
static address_space_t kernel_address_space;
static uint64_t next_asid = 1;

// Track whether PHYS_MAP_BASE is set up
static bool phys_map_ready = false;

// Track the maximum physical address that's mapped to PHYS_MAP_BASE
static paddr_t phys_map_max_addr = 0;

// Recursive mapping entry (PML4 entry 510)
#define RECURSIVE_MAPPING_INDEX 510
#define RECURSIVE_BASE 0xFFFFFF8000000000ULL

/**
 * Convert physical address to virtual for page table access
 * Uses PHYS_MAP_BASE if available, otherwise uses identity mapping
 */
static inline void* phys_to_virt_pt(paddr_t paddr) {
    if (phys_map_ready) {
        // After PHYS_MAP_BASE is ready, ALL physical memory is accessible via it
        // We mapped all 511MB using 2MB huge pages
        return (void*)(paddr + PHYS_MAP_BASE);
    } else {
        // During early boot, use identity mapping (set up by bootloader)
        return (void*)paddr;
    }
}

/**
 * Get page table entry at any level
 *
 * This function handles both identity-mapped and PHYS_MAP_BASE-mapped page tables.
 * During VMM initialization, page tables use identity mapping. After PHYS_MAP_BASE
 * is set up, we use PHYS_MAP_BASE for all page table access.
 */
static uint64_t* get_page_table_entry(uint64_t* pml4, vaddr_t vaddr, int level, bool create) {
    static bool first_call = true;

    // Extract indices
    uint64_t indices[4] = {
        (vaddr >> 39) & 0x1FF,  // PML4
        (vaddr >> 30) & 0x1FF,  // PDP
        (vaddr >> 21) & 0x1FF,  // PD
        (vaddr >> 12) & 0x1FF   // PT
    };

    // Convert PML4 virtual address to physical for access
    paddr_t pml4_phys = (paddr_t)pml4;
    if (phys_map_ready && (uint64_t)pml4 >= PHYS_MAP_BASE) {
        // PML4 is a virtual address using PHYS_MAP_BASE mapping
        pml4_phys = (paddr_t)pml4 - PHYS_MAP_BASE;
    } else if (!phys_map_ready) {
        // During early boot, PML4 is identity-mapped (physical == virtual)
        pml4_phys = (paddr_t)pml4;
    }

    uint64_t* table = phys_to_virt_pt(pml4_phys);

    for (int i = 0; i < level; i++) {
        uint64_t entry = table[indices[i]];

        if (!(entry & VMM_PRESENT)) {
            if (!create) {
                kdebug("VMM: Page table entry not present for vaddr 0x%lx at level %d\n", vaddr, i);
                return NULL;
            }

            // Allocate new page table
            // CRITICAL: During VMM initialization (before PHYS_MAP_BASE is ready),
            // we MUST use low memory (< 128MB) so pages are accessible via identity mapping.
            // After PHYS_MAP_BASE is ready, we can use any memory as it's all mapped.
            kdebug("VMM: Allocating new page table for vaddr 0x%016lx level %d...\n", vaddr, i);

            paddr_t new_table_phys;
            if (!phys_map_ready) {
                // Before PHYS_MAP_BASE is ready, use low memory only
                // This ensures page tables are accessible via identity mapping
                new_table_phys = pmm_alloc_page_low();
                if (new_table_phys == 0) {
                    kerror("VMM: Failed to allocate page table in low memory (phys_map_ready=%d)\n", phys_map_ready);
                    return NULL;
                }
                // Verify it's in low memory
                if (new_table_phys >= (128 * 1024 * 1024)) {
                    kerror("VMM: Page table allocated beyond 128MB: 0x%016lx (phys_map_ready=%d)\n", 
                           new_table_phys, phys_map_ready);
                    pmm_free_page(new_table_phys);
                    return NULL;
                }
            } else {
                // After PHYS_MAP_BASE is ready, we can use any memory
                new_table_phys = pmm_alloc_page();
                if (new_table_phys == 0) {
                    kerror("VMM: Out of memory for page table\n");
                    return NULL;
                }
            }

            kdebug("VMM: Allocated page table at phys 0x%016lx (phys_map_ready=%d)\n", 
                   new_table_phys, phys_map_ready);

            // Access the page table via phys_to_virt_pt (uses PHYS_MAP_BASE or identity mapping)
            uint64_t* virt_table = phys_to_virt_pt(new_table_phys);
            
            // Verify we can access the page table
            if (!virt_table) {
                kerror("VMM: Failed to convert page table phys 0x%016lx to virt (phys_map_ready=%d)\n",
                       new_table_phys, phys_map_ready);
                pmm_free_page(new_table_phys);
                return NULL;
            }
            
            // Clear the page table
            for (int j = 0; j < 512; j++) {
                virt_table[j] = 0;
            }
            kdebug("VMM: Page table cleared successfully\n");

            // Set entry
            kdebug("VMM: Setting PML4 entry %d to 0x%lx...\n", indices[i], new_table_phys | VMM_PRESENT | VMM_WRITE);
            table[indices[i]] = new_table_phys | VMM_PRESENT | VMM_WRITE;
            entry = table[indices[i]];
            kdebug("VMM: Entry set successfully\n");
        }

        // Get next level table using phys_to_virt_pt
        paddr_t next_table_phys = entry & 0xFFFFFFFFF000ULL;
        table = phys_to_virt_pt(next_table_phys);
    }

    return &table[indices[level]];
}

/**
 * Initialize VMM
 */
void vmm_init(void) {
    kinfo("[VMM-ENTRY] vmm_init() called\n");
    kinfo("Initializing Virtual Memory Manager...\n");

    // Disable interrupts during VMM initialization to prevent any interference
    __asm__ volatile("cli");
    kinfo("VMM: Interrupts disabled\n");

    // Get current page table (set up by bootloader)
    kinfo("VMM: Reading CR3...\n");
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    kinfo("VMM: CR3 = 0x%lx\n", cr3);

    // IMPORTANT: We can't use PHYS_MAP_BASE yet because bootloader didn't map it
    // The bootloader only identity-maps first 2GB
    // For now, we'll use identity mapping for page table access
    // CR3 points to PML4 which is in low memory (< 2GB), so we can access it directly
    kinfo("VMM: Setting kernel_address_space.pml4...\n");
    kernel_address_space.pml4 = (uint64_t*)cr3;
    kernel_address_space.asid = 0;
    kernel_address_space.next = NULL;

    kinfo("VMM: kernel_address_space.pml4 = %p\n", kernel_address_space.pml4);
    kinfo("VMM: Setting up physical memory direct map at 0x%lx\n", PHYS_MAP_BASE);

    // Enable PSE (Page Size Extension) in CR4 to support 2MB huge pages
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 4);  // Set PSE bit (bit 4)
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));
    kinfo("VMM: Enabled PSE (Page Size Extension) for 2MB huge pages\n");

    // Map ALL usable physical memory at PHYS_MAP_BASE using 2MB huge pages
    // This drastically reduces page table overhead (512x less memory needed)
    size_t total_pages = pmm_get_total_pages();
    paddr_t memory_to_map = total_pages * PAGE_SIZE;

    kinfo("VMM: Mapping %lu MB at PHYS_MAP_BASE using 2MB huge pages...\n",
          memory_to_map / (1024 * 1024));

    // Map using 2MB huge pages
    // This requires PS (Page Size) bit in Page Directory entries
    #define HUGE_PAGE_SIZE (2 * 1024 * 1024)  // 2MB
    #define VMM_PS 0x80  // Page Size bit for huge pages

    size_t huge_pages_mapped = 0;
    kinfo("VMM: Starting PHYS_MAP_BASE huge page mapping...\n");

    for (paddr_t paddr = 0; paddr < memory_to_map; paddr += HUGE_PAGE_SIZE) {
        vaddr_t vaddr = PHYS_MAP_BASE + paddr;

        // Get PML4 index
        uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
        uint64_t pdp_idx = (vaddr >> 30) & 0x1FF;
        uint64_t pd_idx = (vaddr >> 21) & 0x1FF;

        // Access PML4 via identity mapping
        uint64_t* pml4 = (uint64_t*)cr3;

        // Get or create PDP
        if (!(pml4[pml4_idx] & VMM_PRESENT)) {
            // Use low memory for these early page tables
            paddr_t pdp_phys = pmm_alloc_page_low();
            if (pdp_phys == 0) {
                kerror("VMM: Out of memory for PDP\n");
                break;
            }
            uint64_t* pdp = (uint64_t*)pdp_phys;
            for (int i = 0; i < 512; i++) pdp[i] = 0;
            pml4[pml4_idx] = pdp_phys | VMM_PRESENT | VMM_WRITE;
        }

        // Get PDP
        uint64_t* pdp = (uint64_t*)(pml4[pml4_idx] & 0xFFFFFFFFF000ULL);

        // Get or create PD
        if (!(pdp[pdp_idx] & VMM_PRESENT)) {
            // Use low memory for these early page tables
            paddr_t pd_phys = pmm_alloc_page_low();
            if (pd_phys == 0) {
                kerror("VMM: Out of memory for PD\n");
                break;
            }
            uint64_t* pd = (uint64_t*)pd_phys;
            for (int i = 0; i < 512; i++) pd[i] = 0;
            pdp[pdp_idx] = pd_phys | VMM_PRESENT | VMM_WRITE;
        }

        // Get PD and create 2MB huge page mapping
        // Note: Not setting NX bit to avoid issues with NX support
        uint64_t* pd = (uint64_t*)(pdp[pdp_idx] & 0xFFFFFFFFF000ULL);
        pd[pd_idx] = paddr | VMM_PRESENT | VMM_WRITE | VMM_PS;

        huge_pages_mapped++;

        // Show progress every 64MB
        if (huge_pages_mapped % 32 == 0) {
            kinfo("VMM: Mapped %lu MB...\n", (huge_pages_mapped * 2));
        }
    }

    kinfo("VMM: Successfully mapped %lu MB using %lu huge pages\n",
          (huge_pages_mapped * 2), huge_pages_mapped);

    // Now PHYS_MAP_BASE is ready - switch to using it for page table access
    phys_map_ready = true;
    phys_map_max_addr = memory_to_map;  // Track maximum mapped address
    kinfo("VMM: PHYS_MAP_BASE is now ready for use (mapped up to 0x%016lx)\n", phys_map_max_addr);
    //bootstrap_disable();

    // Update kernel_address_space.pml4 to use PHYS_MAP_BASE
    kernel_address_space.pml4 = (uint64_t*)(cr3 + PHYS_MAP_BASE);
    kinfo("VMM: Updated kernel_address_space.pml4 to %p\n", kernel_address_space.pml4);

    kprintf("[INFO] VMM initialized with kernel page tables at 0x%lx\n", cr3);

    // Re-enable interrupts
    __asm__ volatile("sti");
    kinfo("VMM: Interrupts re-enabled\n");

    kprintf("[INFO] VMM initialization complete\n");
}

/**
 * Create a new address space
 */
address_space_t* vmm_create_address_space(void) {
    // Allocate address space structure
    address_space_t* as = (address_space_t*)kmalloc(sizeof(address_space_t));
    if (!as) {
        kerror("VMM: Failed to allocate address space structure\n");
        return NULL;
    }
    
    // Allocate PML4
    paddr_t pml4_phys = pmm_alloc_page();
    if (pml4_phys == 0) {
        kfree(as);
        return NULL;
    }
    
    as->pml4 = (uint64_t*)(pml4_phys + PHYS_MAP_BASE);
    as->asid = next_asid++;
    as->next = NULL;
    
    // Clear PML4
    for (int i = 0; i < 512; i++) {
        as->pml4[i] = 0;
    }
    
    // Copy kernel mappings (upper half)
    for (int i = 256; i < 512; i++) {
        as->pml4[i] = kernel_address_space.pml4[i];
    }
    
    return as;
}

/**
 * Destroy an address space
 */
void vmm_destroy_address_space(address_space_t* as) {
    if (!as || as == &kernel_address_space) {
        return;
    }
    
    // Free user-space page tables (lower half only)
    for (int i = 0; i < 256; i++) {
        if (as->pml4[i] & VMM_PRESENT) {
            paddr_t pdp_phys = as->pml4[i] & 0xFFFFFFFFF000ULL;
            uint64_t* pdp = (uint64_t*)(pdp_phys + PHYS_MAP_BASE);
            
            for (int j = 0; j < 512; j++) {
                if (pdp[j] & VMM_PRESENT) {
                    paddr_t pd_phys = pdp[j] & 0xFFFFFFFFF000ULL;
                    uint64_t* pd = (uint64_t*)(pd_phys + PHYS_MAP_BASE);
                    
                    for (int k = 0; k < 512; k++) {
                        if (pd[k] & VMM_PRESENT) {
                            paddr_t pt_phys = pd[k] & 0xFFFFFFFFF000ULL;
                            pmm_free_page(pt_phys);
                        }
                    }
                    
                    pmm_free_page(pd_phys);
                }
            }
            
            pmm_free_page(pdp_phys);
        }
    }
    
    // Free PML4
    paddr_t pml4_phys = (paddr_t)as->pml4 - PHYS_MAP_BASE;
    pmm_free_page(pml4_phys);
    
    // Free address space structure
    kfree(as);
}

/**
 * Switch to an address space
 */
void vmm_switch_address_space(address_space_t* as) {
    paddr_t pml4_phys = (paddr_t)as->pml4 - PHYS_MAP_BASE;
    __asm__ volatile("mov %0, %%cr3" :: "r"(pml4_phys));
}

/**
 * Map a virtual page to a physical page
 */
int vmm_map_page(address_space_t* as, vaddr_t vaddr, paddr_t paddr, uint64_t flags) {
    if (!as) {
        as = &kernel_address_space;
    }
    
    // Get page table entry - need to use physical address for PML4 if using PHYS_MAP_BASE
    uint64_t* pml4_virt = as->pml4;
    
    // If PML4 is using PHYS_MAP_BASE, extract physical address for get_page_table_entry
    // get_page_table_entry expects either identity-mapped or will handle PHYS_MAP_BASE
    uint64_t* pte = get_page_table_entry(pml4_virt, vaddr, 4, true);
    if (!pte) {
        kerror("VMM: Failed to get PTE for mapping vaddr 0x%016lx paddr 0x%016lx\n", vaddr, paddr);
        return -1;
    }
    
    // Set entry
    *pte = (paddr & 0xFFFFFFFFF000ULL) | flags;
    
    // Flush TLB
    vmm_flush_tlb_single(vaddr);
    
    return 0;
}

/**
 * Unmap a virtual page
 */
int vmm_unmap_page(address_space_t* as, vaddr_t vaddr) {
    if (!as) {
        as = &kernel_address_space;
    }
    
    uint64_t* pte = get_page_table_entry(as->pml4, vaddr, 4, false);
    if (!pte || !(*pte & VMM_PRESENT)) {
        return -1;  // Page not mapped
    }
    
    // Get physical address before unmapping
    paddr_t paddr = *pte & 0xFFFFFFFFF000ULL;
    
    // Free the physical page (will decrement reference count)
    extern void pmm_free_page(paddr_t page);
    pmm_free_page(paddr);
    
    *pte = 0;
    vmm_flush_tlb_single(vaddr);
    
    return 0;
}

/**
 * Get physical address for virtual address
 */
paddr_t vmm_get_physical(address_space_t* as, vaddr_t vaddr) {
    if (!as) {
        as = &kernel_address_space;
    }
    
    uint64_t* pte = get_page_table_entry(as->pml4, vaddr, 4, false);
    if (!pte || !(*pte & VMM_PRESENT)) {
        kdebug("VMM: Failed to get physical address for vaddr 0x%lx (pte not found or not present)\n", vaddr);
        return 0;
    }
    
    paddr_t page_phys = *pte & 0xFFFFFFFFF000ULL;
    uint64_t offset = vaddr & 0xFFF;
    
    return page_phys + offset;
}

/**
 * Map multiple contiguous pages
 */
int vmm_map_pages(address_space_t* as, vaddr_t vaddr, paddr_t paddr, size_t count, uint64_t flags) {
    for (size_t i = 0; i < count; i++) {
        if (vmm_map_page(as, vaddr + (i * PAGE_SIZE), paddr + (i * PAGE_SIZE), flags) != 0) {
            // Rollback
            for (size_t j = 0; j < i; j++) {
                vmm_unmap_page(as, vaddr + (j * PAGE_SIZE));
            }
            return -1;
        }
    }
    return 0;
}

/**
 * Unmap multiple contiguous pages
 */
int vmm_unmap_pages(address_space_t* as, vaddr_t vaddr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        vmm_unmap_page(as, vaddr + (i * PAGE_SIZE));
    }
    return 0;
}

/**
 * Flush TLB for a single address
 */
void vmm_flush_tlb_single(vaddr_t vaddr) {
    __asm__ volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
}

/**
 * Flush entire TLB
 */
void vmm_flush_tlb_all(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");
}

/**
 * Get kernel address space
 */
address_space_t* vmm_get_kernel_address_space(void) {
    return &kernel_address_space;
}

/**
 * Mark a page as Copy-on-Write (remove write permission, set CoW flag)
 */
int vmm_mark_cow(address_space_t* as, vaddr_t vaddr) {
    if (!as) {
        as = &kernel_address_space;
    }
    
    uint64_t* pte = get_page_table_entry(as->pml4, vaddr, 4, false);
    if (!pte || !(*pte & VMM_PRESENT)) {
        return -1;
    }
    
    // Increment reference count for the physical page
    paddr_t paddr = *pte & 0xFFFFFFFFF000ULL;
    extern void pmm_ref_page(paddr_t page);
    pmm_ref_page(paddr);
    
    // Remove write permission and set CoW flag
    uint64_t flags = *pte & ~VMM_WRITE;
    flags |= VMM_COW;
    *pte = flags;
    
    vmm_flush_tlb_single(vaddr);
    
    return 0;
}

/**
 * Get current address space (from current process)
 */
static address_space_t* vmm_get_current_address_space(void) {
    extern process_t* process_get_current(void);
    extern address_space_t* process_get_address_space(process_t* proc);
    process_t* proc = process_get_current();
    if (proc) {
        return process_get_address_space(proc);
    }
    return &kernel_address_space;
}

/**
 * Handle Copy-on-Write page fault
 */
int vmm_handle_cow_fault(vaddr_t vaddr) {
    address_space_t* as = vmm_get_current_address_space();
    
    uint64_t* pte = get_page_table_entry(as->pml4, vaddr, 4, false);
    if (!pte || !(*pte & VMM_PRESENT)) {
        return -1;  // Not a CoW fault
    }
    
    // Check if this is a CoW page
    if (!(*pte & VMM_COW)) {
        return -1;  // Not a CoW fault
    }
    
    // Get the original physical page
    paddr_t old_paddr = *pte & 0xFFFFFFFFF000ULL;
    
    // Allocate a new physical page
    extern paddr_t pmm_alloc_page(void);
    paddr_t new_paddr = pmm_alloc_page();
    if (new_paddr == 0) {
        kerror("CoW: Out of memory\n");
        return -1;
    }
    
    // Copy page contents
    extern uint8_t* phys_to_virt(paddr_t paddr);
    uint8_t* old_virt = (uint8_t*)(old_paddr + PHYS_MAP_BASE);
    uint8_t* new_virt = (uint8_t*)(new_paddr + PHYS_MAP_BASE);
    
    extern void* memcpy(void* dest, const void* src, size_t n);
    memcpy(new_virt, old_virt, PAGE_SIZE);
    
    // Update page table entry: restore write permission, clear CoW flag
    uint64_t flags = *pte & ~VMM_COW;
    flags |= VMM_WRITE;
    *pte = (new_paddr & 0xFFFFFFFFF000ULL) | flags;
    
    // Decrement reference count on old page
    extern void pmm_free_page(paddr_t page);
    pmm_free_page(old_paddr);
    
    vmm_flush_tlb_single(vaddr);
    
    return 0;
}

