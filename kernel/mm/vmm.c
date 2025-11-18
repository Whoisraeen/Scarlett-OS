/**
 * @file vmm.c
 * @brief Virtual Memory Manager implementation
 */

#include "../include/types.h"
#include "../include/config.h"
#include "../include/mm/vmm.h"
#include "../include/mm/pmm.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

// Kernel address space
static address_space_t kernel_address_space;
static uint64_t next_asid = 1;

/**
 * Get page table entry at any level
 * 
 * This function handles both identity-mapped and PHYS_MAP_BASE-mapped page tables.
 * During VMM initialization, page tables are identity-mapped. After PHYS_MAP_BASE
 * is set up, we use PHYS_MAP_BASE for all page table access.
 */
static uint64_t* get_page_table_entry(uint64_t* pml4, vaddr_t vaddr, int level, bool create) {
    // Extract indices
    uint64_t indices[4] = {
        (vaddr >> 39) & 0x1FF,  // PML4
        (vaddr >> 30) & 0x1FF,  // PDP
        (vaddr >> 21) & 0x1FF,  // PD
        (vaddr >> 12) & 0x1FF   // PT
    };
    
    // For now, always use identity mapping (pml4 is in low memory)
    // TODO: Support PHYS_MAP_BASE after it's fully verified
    uint64_t* table = pml4;
    
    for (int i = 0; i < level; i++) {
        uint64_t entry = table[indices[i]];
        
        if (!(entry & VMM_PRESENT)) {
            if (!create) {
                kdebug("VMM: Page table entry not present for vaddr 0x%lx at level %d\n", vaddr, i);
                return NULL;
            }
            
            // Allocate new page table
            paddr_t new_table = pmm_alloc_page();
            if (new_table == 0) {
                kerror("VMM: Out of memory for page table\n");
                return NULL;
            }
            
            // Clear new table - use identity mapping for now
            uint64_t* virt_table = (uint64_t*)new_table;
            for (int j = 0; j < 512; j++) {
                virt_table[j] = 0;
            }
            
            // Set entry
            table[indices[i]] = new_table | VMM_PRESENT | VMM_WRITE;
            entry = table[indices[i]];
        }
        
        // Get next level table - use identity mapping
        paddr_t next_table_phys = entry & 0xFFFFFFFFF000ULL;
        table = (uint64_t*)next_table_phys;  // Identity mapped
    }
    
    return &table[indices[level]];
}

/**
 * Initialize VMM
 */
void vmm_init(void) {
    kinfo("[VMM-ENTRY] vmm_init() called\n");
    kinfo("Initializing Virtual Memory Manager...\n");

    // Get current page table (set up by bootloader)
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    kdebug("VMM: CR3 = 0x%lx\n", cr3);

    // IMPORTANT: We can't use PHYS_MAP_BASE yet because bootloader didn't map it
    // The bootloader only identity-maps first 2GB
    // For now, we'll use identity mapping for page table access
    // CR3 points to PML4 which is in low memory (< 2GB), so we can access it directly
    kernel_address_space.pml4 = (uint64_t*)cr3;
    kernel_address_space.asid = 0;
    kernel_address_space.next = NULL;

    kdebug("VMM: kernel_address_space.pml4 = %p\n", kernel_address_space.pml4);
    kdebug("VMM: Setting up physical memory direct map at 0x%lx\n", PHYS_MAP_BASE);

    // Map the first 4GB of physical memory to PHYS_MAP_BASE
    // This allows us to access any physical address by adding PHYS_MAP_BASE
    // We'll map 4GB using 2MB pages (2048 pages = 4GB)
    uint64_t phys_addr = 0;
    for (uint64_t i = 0; i < 2048; i++) {
        vaddr_t virt_addr = PHYS_MAP_BASE + phys_addr;

        // Get PML4 index for this virtual address
        uint64_t pml4_idx = (virt_addr >> 39) & 0x1FF;
        uint64_t pdp_idx = (virt_addr >> 30) & 0x1FF;
        uint64_t pd_idx = (virt_addr >> 21) & 0x1FF;

        // Ensure PML4 entry exists
        if (!(kernel_address_space.pml4[pml4_idx] & VMM_PRESENT)) {
            paddr_t pdp_phys = pmm_alloc_page();
            if (pdp_phys == 0) {
                kerror("VMM: Out of memory during PHYS_MAP setup\n");
                kpanic("VMM initialization failed");
            }
            // Clear the new PDP
            uint64_t* pdp_virt = (uint64_t*)pdp_phys;  // Identity mapped
            for (int j = 0; j < 512; j++) pdp_virt[j] = 0;
            kernel_address_space.pml4[pml4_idx] = pdp_phys | VMM_PRESENT | VMM_WRITE;
        }

        // Get PDP
        paddr_t pdp_phys = kernel_address_space.pml4[pml4_idx] & 0xFFFFFFFFF000ULL;
        uint64_t* pdp = (uint64_t*)pdp_phys;  // Identity mapped

        // Ensure PDP entry exists
        if (!(pdp[pdp_idx] & VMM_PRESENT)) {
            paddr_t pd_phys = pmm_alloc_page();
            if (pd_phys == 0) {
                kerror("VMM: Out of memory during PHYS_MAP setup\n");
                kpanic("VMM initialization failed");
            }
            // Clear the new PD
            uint64_t* pd_virt = (uint64_t*)pd_phys;  // Identity mapped
            for (int j = 0; j < 512; j++) pd_virt[j] = 0;
            pdp[pdp_idx] = pd_phys | VMM_PRESENT | VMM_WRITE;
        }

        // Get PD
        paddr_t pd_phys = pdp[pdp_idx] & 0xFFFFFFFFF000ULL;
        uint64_t* pd = (uint64_t*)pd_phys;  // Identity mapped

        // Map 2MB page
        pd[pd_idx] = phys_addr | VMM_PRESENT | VMM_WRITE | VMM_HUGE;

        phys_addr += 0x200000;  // 2MB
    }

    // Flush TLB to activate new mappings
    vmm_flush_tlb_all();

    kdebug("VMM: Physical memory direct map complete (4GB mapped)\n");

    // For now, keep using identity mapping for pml4
    // TODO: Switch to PHYS_MAP_BASE after verifying it works
    // kernel_address_space.pml4 = (uint64_t*)(cr3 + PHYS_MAP_BASE);
    
    kprintf("[INFO] VMM initialized with kernel page tables at 0x%lx\n", cr3);
    
    // Force serial flush by writing directly
    extern void serial_putchar(char c);
    const char* msg = "VMM: Testing direct serial output\n";
    for (const char* p = msg; *p; p++) {
        serial_putchar(*p);
    }
    
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
    
    // Get page table entry
    uint64_t* pte = get_page_table_entry(as->pml4, vaddr, 4, true);
    if (!pte) {
        kerror("VMM: Failed to get PTE for mapping vaddr 0x%lx\n", vaddr);
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
    if (!pte) {
        kerror("VMM: Failed to get PTE for unmapping vaddr 0x%lx\n", vaddr);
        return -1;
    }
    
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

