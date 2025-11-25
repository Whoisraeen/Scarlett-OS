/**
 * @file arm64_mmu.c
 * @brief ARM64 Memory Management Unit Implementation
 * @details Full 4-level page table support with dynamic allocation
 */

#include "arm64_hal.h"
#include <string.h>
#include "../../include/mm/pmm.h"
#include "../../include/mm/heap.h"
#include "../../include/kprintf.h"

// Page table levels
#define PGD_SHIFT 39
#define PUD_SHIFT 30
#define PMD_SHIFT 21
#define PTE_SHIFT 12

#define PTRS_PER_TABLE 512
#define PAGE_SIZE 4096

// Initial Boot Page Tables (Static)
// We use these only during early boot until PMM is ready
static uint64_t boot_pgd[PTRS_PER_TABLE] __attribute__((aligned(4096)));
static uint64_t boot_pud[PTRS_PER_TABLE] __attribute__((aligned(4096)));
static uint64_t boot_pmd[PTRS_PER_TABLE] __attribute__((aligned(4096)));
// No PTE for boot 2MB identity map

// Helper: Get physical address of a pointer (Assuming linear mapping or early boot)
// In early boot, we are identity mapped, so virt == phys
static uint64_t virt_to_phys(void* vaddr) {
    return (uint64_t)vaddr;
}

// Helper: Get virtual address (identity)
static void* phys_to_virt(uint64_t paddr) {
    return (void*)paddr;
}

// Read ID_AA64MMFR0_EL1 to check physical address range
static inline uint64_t read_sysreg_id_aa64mmfr0(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(val));
    return val;
}

// Read system register
static inline uint64_t read_sysreg_sctlr(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, sctlr_el1" : \"=r\"(val));
    return val;
}
static inline uint64_t read_sysreg_tcr(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, tcr_el1" : \"=r\"(val));
    return val;
}
static inline uint64_t read_sysreg_ttbr0(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, ttbr0_el1" : \"=r\"(val));
    return val;
}

// Write system register
static inline void write_sysreg_sctlr(uint64_t val) {
    __asm__ volatile("msr sctlr_el1, %0" :: \"r\"(val));
    __asm__ volatile("isb");
}
static inline void write_sysreg_tcr(uint64_t val) {
    __asm__ volatile("msr tcr_el1, %0" :: \"r\"(val));
    __asm__ volatile("isb");
}
static inline void write_sysreg_ttbr0(uint64_t val) {
    __asm__ volatile("msr ttbr0_el1, %0" :: \"r\"(val));
    __asm__ volatile("isb");
}
static inline void write_sysreg_mair(uint64_t val) {
    __asm__ volatile("msr mair_el1, %0" :: \"r\"(val));
    __asm__ volatile("isb");
}

void arm64_mmu_init(void) {
    // Clear boot page tables
    memset(boot_pgd, 0, sizeof(boot_pgd));
    memset(boot_pud, 0, sizeof(boot_pud));
    memset(boot_pmd, 0, sizeof(boot_pmd));
    
    // Set up MAIR (Memory Attribute Indirection Register)
    uint64_t mair = (MAIR_DEVICE_nGnRnE << 0) |  // Index 0: Device
                    (MAIR_NORMAL_NC << 8) |       // Index 1: Normal non-cacheable
                    (MAIR_NORMAL << 16);          // Index 2: Normal cacheable
    write_sysreg_mair(mair);
    
    // Check supported Physical Address Range (PARange)
    uint64_t mmfr0 = read_sysreg_id_aa64mmfr0();
    uint64_t parange = mmfr0 & 0xF;
    uint64_t ips = 0;
    
    // Map ID_AA64MMFR0_EL1.PARange to TCR_EL1.IPS
    switch (parange) {
        case 0: ips = 0; break; // 32-bit (4GB)
        case 1: ips = 1; break; // 36-bit (64GB)
        case 2: ips = 2; break; // 40-bit (1TB)
        case 3: ips = 3; break; // 42-bit (4TB)
        case 4: ips = 4; break; // 44-bit (16TB)
        case 5: ips = 5; break; // 48-bit (256TB)
        case 6: ips = 6; break; // 52-bit (4PB)
        default: ips = 5; break; // Default to 48-bit if unknown
    }
    
    // Set up TCR (Translation Control Register)
    // T0SZ=16 (48-bit), TG0=4KB, IPS=Dynamic, Inner/Outer WB WA
    uint64_t tcr = (16UL << 0) |   // T0SZ = 16 (48-bit VA)
                   (1UL << 8) |    // IRGN0 = Normal, Write-Back Write-Allocate
                   (1UL << 10) |   // ORGN0 = Normal, Write-Back Write-Allocate
                   (3UL << 12) |   // SH0 = Inner Shareable
                   (0UL << 14) |   // TG0 = 4KB
                   (ips << 32);    // IPS = Dynamic
    write_sysreg_tcr(tcr);
    
    // Setup Boot Identity Map for first 1GB (0x0 - 0x40000000)
    // PGD[0] -> PUD
    boot_pgd[0] = virt_to_phys(boot_pud) | PTE_VALID | PTE_TABLE;
    
    // PUD[0] -> PMD
    boot_pud[0] = virt_to_phys(boot_pmd) | PTE_VALID | PTE_TABLE;
    // PUD[1] -> PMD (Second GB? No, just 1GB for now)
    
    // PMD entries (2MB blocks)
    // Map 512 entries * 2MB = 1GB
    for (int i = 0; i < 512; i++) {
        uint64_t phys = i * 0x200000;
        boot_pmd[i] = phys | PTE_VALID | PTE_AF | PTE_BLOCK | (2UL << 2) | PTE_ISH | PTE_RO; 
        // (2UL << 2) is AttrIndx 2 (Normal Cacheable)
        // PTE_RO? No, we need RW.
        boot_pmd[i] &= ~PTE_RO;
    }
    
    // Set TTBR0 to point to our boot page table
    write_sysreg_ttbr0(virt_to_phys(boot_pgd));
    
    // Invalidate TLB
    __asm__ volatile("tlbi vmalle1is");
    __asm__ volatile("dsb ish");
    __asm__ volatile("isb");
}

void arm64_mmu_enable(void) {
    // Enable MMU
    uint64_t sctlr = read_sysreg_sctlr();
    sctlr |= SCTLR_EL1_M | SCTLR_EL1_C | SCTLR_EL1_I;
    write_sysreg_sctlr(sctlr);
    
    // Ensure changes are visible
    __asm__ volatile("isb");
}

// Helper: Allocate a page table page (physical address)
static uint64_t alloc_pt_page(void) {
    // If PMM is ready, use it. Otherwise panic or use static pool (not implemented)
    // Assuming PMM is initialized before dynamic mappings are needed beyond boot
    void* page = pmm_alloc_page();
    if (!page) {
        kerror("ARM64 MMU: Failed to allocate page table\n");
        return 0;
    }
    memset(phys_to_virt((uint64_t)page), 0, PAGE_SIZE);
    return (uint64_t)page;
}

void arm64_mmu_map(uint64_t vaddr, uint64_t paddr, uint64_t size, uint64_t flags) {
    uint64_t current_vaddr = vaddr;
    uint64_t current_paddr = paddr;
    uint64_t remaining = size;
    
    // Get current root
    uint64_t root_phys = read_sysreg_ttbr0();
    uint64_t* pgd = (uint64_t*)phys_to_virt(root_phys);
    
    while (remaining > 0) {
        uint64_t pgd_idx = (current_vaddr >> PGD_SHIFT) & 0x1FF;
        uint64_t pud_idx = (current_vaddr >> PUD_SHIFT) & 0x1FF;
        uint64_t pmd_idx = (current_vaddr >> PMD_SHIFT) & 0x1FF;
        uint64_t pte_idx = (current_vaddr >> PTE_SHIFT) & 0x1FF;
        
        // 1. PGD -> PUD
        if (!(pgd[pgd_idx] & PTE_VALID)) {
            uint64_t pud_phys = alloc_pt_page();
            if (!pud_phys) return;
            pgd[pgd_idx] = pud_phys | PTE_VALID | PTE_TABLE;
        }
        uint64_t* pud = (uint64_t*)phys_to_virt(pgd[pgd_idx] & 0xFFFFFFFFF000ULL);
        
        // 2. PUD -> PMD
        if (!(pud[pud_idx] & PTE_VALID)) {
             uint64_t pmd_phys = alloc_pt_page();
             if (!pmd_phys) return;
             pud[pud_idx] = pmd_phys | PTE_VALID | PTE_TABLE;
        }
        uint64_t* pmd = (uint64_t*)phys_to_virt(pud[pud_idx] & 0xFFFFFFFFF000ULL);
        
        // 3. PMD -> PTE (or Block)
        // Determine if we can use 2MB block
        // Conditions: 2MB aligned vaddr, 2MB aligned paddr, remaining >= 2MB
        if ((current_vaddr & 0x1FFFFF) == 0 && 
            (current_paddr & 0x1FFFFF) == 0 && 
            remaining >= 0x200000) {
            
            // Use 2MB Block
            pmd[pmd_idx] = current_paddr | flags | PTE_VALID | PTE_AF | PTE_BLOCK;
            
            current_vaddr += 0x200000;
            current_paddr += 0x200000;
            remaining -= 0x200000;
            continue;
        }
        
        // Use 4KB Pages
        if (!(pmd[pmd_idx] & PTE_VALID)) {
             uint64_t pte_phys = alloc_pt_page();
             if (!pte_phys) return;
             pmd[pmd_idx] = pte_phys | PTE_VALID | PTE_TABLE;
        } else if ((pmd[pmd_idx] & 3) == 1) { 
             // It's a block! We must split it?
             // For now assume we don't remap blocks as pages without unmap first
        }
        
        uint64_t* pte = (uint64_t*)phys_to_virt(pmd[pmd_idx] & 0xFFFFFFFFF000ULL);
        
        pte[pte_idx] = current_paddr | flags | PTE_VALID | PTE_AF | PTE_PAGE;
        
        current_vaddr += PAGE_SIZE;
        current_paddr += PAGE_SIZE;
        remaining -= PAGE_SIZE;
    }
    
    // Flush TLB
    __asm__ volatile("tlbi vmalle1is");
    __asm__ volatile("dsb ish");
    __asm__ volatile("isb");
}

void arm64_mmu_unmap(uint64_t vaddr, uint64_t size) {
    uint64_t current_vaddr = vaddr;
    uint64_t remaining = size;
    
    uint64_t root_phys = read_sysreg_ttbr0();
    uint64_t* pgd = (uint64_t*)phys_to_virt(root_phys);
    
    while (remaining > 0) {
        uint64_t pgd_idx = (current_vaddr >> PGD_SHIFT) & 0x1FF;
        uint64_t pud_idx = (current_vaddr >> PUD_SHIFT) & 0x1FF;
        uint64_t pmd_idx = (current_vaddr >> PMD_SHIFT) & 0x1FF;
        uint64_t pte_idx = (current_vaddr >> PTE_SHIFT) & 0x1FF;
        
        if (!(pgd[pgd_idx] & PTE_VALID)) {
            // Skip huge range
            uint64_t next = (current_vaddr & ~((1UL<<PGD_SHIFT)-1)) + (1UL<<PGD_SHIFT);
            uint64_t skip = next - current_vaddr;
            if (skip > remaining) skip = remaining;
            current_vaddr += skip;
            remaining -= skip;
            continue;
        }
        uint64_t* pud = (uint64_t*)phys_to_virt(pgd[pgd_idx] & 0xFFFFFFFFF000ULL);
        
        if (!(pud[pud_idx] & PTE_VALID)) {
             uint64_t next = (current_vaddr & ~((1UL<<PUD_SHIFT)-1)) + (1UL<<PUD_SHIFT);
             uint64_t skip = next - current_vaddr;
             if (skip > remaining) skip = remaining;
             current_vaddr += skip;
             remaining -= skip;
             continue;
        }
        uint64_t* pmd = (uint64_t*)phys_to_virt(pud[pud_idx] & 0xFFFFFFFFF000ULL);
        
        if (!(pmd[pmd_idx] & PTE_VALID)) {
             uint64_t next = (current_vaddr & ~((1UL<<PMD_SHIFT)-1)) + (1UL<<PMD_SHIFT);
             uint64_t skip = next - current_vaddr;
             if (skip > remaining) skip = remaining;
             current_vaddr += skip;
             remaining -= skip;
             continue;
        }
        
        // Check if Block
        if ((pmd[pmd_idx] & 3) == 1) {
            // It's a 2MB block.
            // Check if we are unmapping the whole block or just a part
            if ((current_vaddr & 0x1FFFFF) == 0 && remaining >= 0x200000) {
                // Unmap whole block
                pmd[pmd_idx] = 0;
                current_vaddr += 0x200000;
                remaining -= 0x200000;
                continue;
            } else {
                // Partial unmap! Split the block.
                uint64_t block_entry = pmd[pmd_idx];
                uint64_t block_phys = block_entry & 0xFFFFFFFFF000ULL;
                // Preserve flags relevant to 4KB pages (AttrIndx, SH, AP, NS, AF, Valid)
                // Block entry format: [63:52] Ignored, [51:48] PXN, XN, [47:12] Output Address, [11:10] nG, AF, SH, AP, NS, AttrIndx, Type (1)
                // Page entry format: [63:52] Ignored, [51:48] PXN, XN, [47:12] Output Address, [11:10] nG, AF, SH, AP, NS, AttrIndx, Type (3)
                // The flags to preserve are AttrIndx (bits 2-4), NS (bit 5), AP (bits 6-7), SH (bits 8-9), AF (bit 10), nG (bit 11), XN (bit 54), PXN (bit 53)
                // A simpler way is to mask out the block type bit and then add page type bit.
                // The block_flags should be the flags that are common to both block and page descriptors,
                // excluding the type bits (0-1) and the output address.
                uint64_t block_flags = (block_entry & ~0x3ULL) & ~0xFFFFFFFFF000ULL; // Clear type bits and address bits
                
                // Allocate a new PTE page
                uint64_t pte_phys = alloc_pt_page();
                if (!pte_phys) {
                    kerror("ARM64 MMU: Failed to allocate page table for split\n");
                    return;
                }
                
                uint64_t* pte = (uint64_t*)phys_to_virt(pte_phys);
                
                // Fill PTE with 4KB pages mapping the original 2MB block
                // The block_phys is the base address of the 2MB block.
                // The virtual address for this PMD entry starts at (current_vaddr & ~0x1FFFFFULL).
                uint64_t block_vaddr_base = current_vaddr & ~0x1FFFFFULL;
                for (int i = 0; i < 512; i++) {
                    pte[i] = (block_phys + (i * PAGE_SIZE)) | block_flags | PTE_VALID | PTE_PAGE | PTE_AF;
                }
                
                // Update PMD to point to new PTE table
                pmd[pmd_idx] = pte_phys | PTE_VALID | PTE_TABLE;
                
                // Now fall through to PTE handling logic below to unmap the specific page(s)
            }
        }
        
        uint64_t* pte = (uint64_t*)phys_to_virt(pmd[pmd_idx] & 0xFFFFFFFFF000ULL);
        pte[pte_idx] = 0;
        
        current_vaddr += PAGE_SIZE;
        remaining -= PAGE_SIZE;
    }
    
    __asm__ volatile("tlbi vmalle1is");
    __asm__ volatile("dsb ish");
    __asm__ volatile("isb");
}