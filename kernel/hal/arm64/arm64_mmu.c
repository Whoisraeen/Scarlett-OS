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
    
    // Set up TCR (Translation Control Register)
    // T0SZ=16 (48-bit), TG0=4KB, IPS=48-bit, Inner/Outer WB WA
    uint64_t tcr = (16UL << 0) |   // T0SZ = 16 (48-bit VA)
                   (1UL << 8) |    // IRGN0 = Normal, Write-Back Write-Allocate
                   (1UL << 10) |   // ORGN0 = Normal, Write-Back Write-Allocate
                   (3UL << 12) |   // SH0 = Inner Shareable
                   (0UL << 14) |   // TG0 = 4KB
                   (5UL << 32);    // IPS = 48-bit (TODO: Check ID_AA64MMFR0_EL1)
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
            // It's a 2MB block. Unmap it.
            pmd[pmd_idx] = 0;
            
            // TODO: If partial unmap of a block, we should split it?
            // For now assuming unmap is aligned or we just unmap the whole block
            current_vaddr += 0x200000;
            if (remaining < 0x200000) remaining = 0; else remaining -= 0x200000;
            continue;
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