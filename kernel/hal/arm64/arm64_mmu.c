/**
 * @file arm64_mmu.c
 * @brief ARM64 Memory Management Unit Implementation
 */

#include "arm64_hal.h"
#include <string.h>

// Page table levels
#define PGD_SHIFT 39
#define PUD_SHIFT 30
#define PMD_SHIFT 21
#define PTE_SHIFT 12

#define PTRS_PER_TABLE 512

// Page tables (simplified - should be dynamically allocated)
static uint64_t pgd[PTRS_PER_TABLE] __attribute__((aligned(4096)));
static uint64_t pud[PTRS_PER_TABLE] __attribute__((aligned(4096)));
static uint64_t pmd[PTRS_PER_TABLE] __attribute__((aligned(4096)));
static uint64_t pte[PTRS_PER_TABLE] __attribute__((aligned(4096)));

// Read system register
static inline uint64_t read_sysreg(const char* reg) {
    uint64_t val;
    if (strcmp(reg, "sctlr_el1") == 0) {
        __asm__ volatile("mrs %0, sctlr_el1" : "=r"(val));
    } else if (strcmp(reg, "tcr_el1") == 0) {
        __asm__ volatile("mrs %0, tcr_el1" : "=r"(val));
    } else if (strcmp(reg, "ttbr0_el1") == 0) {
        __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(val));
    }
    return val;
}

// Write system register
static inline void write_sysreg(const char* reg, uint64_t val) {
    if (strcmp(reg, "sctlr_el1") == 0) {
        __asm__ volatile("msr sctlr_el1, %0" :: "r"(val));
    } else if (strcmp(reg, "tcr_el1") == 0) {
        __asm__ volatile("msr tcr_el1, %0" :: "r"(val));
    } else if (strcmp(reg, "ttbr0_el1") == 0) {
        __asm__ volatile("msr ttbr0_el1, %0" :: "r"(val));
    } else if (strcmp(reg, "mair_el1") == 0) {
        __asm__ volatile("msr mair_el1, %0" :: "r"(val));
    }
    __asm__ volatile("isb");
}

void arm64_mmu_init(void) {
    // Clear page tables
    memset(pgd, 0, sizeof(pgd));
    memset(pud, 0, sizeof(pud));
    memset(pmd, 0, sizeof(pmd));
    memset(pte, 0, sizeof(pte));
    
    // Set up MAIR (Memory Attribute Indirection Register)
    uint64_t mair = (MAIR_DEVICE_nGnRnE << 0) |  // Index 0: Device
                    (MAIR_NORMAL_NC << 8) |       // Index 1: Normal non-cacheable
                    (MAIR_NORMAL << 16);          // Index 2: Normal cacheable
    write_sysreg("mair_el1", mair);
    
    // Set up TCR (Translation Control Register)
    uint64_t tcr = (16UL << 0) |   // T0SZ = 16 (48-bit VA)
                   (0UL << 8) |    // IRGN0 = Normal, Inner Write-Back
                   (0UL << 10) |   // ORGN0 = Normal, Outer Write-Back
                   (3UL << 12) |   // SH0 = Inner Shareable
                   (2UL << 16) |   // TG0 = 4KB granule
                   (25UL << 32);   // IPS = 48-bit physical address
    write_sysreg("tcr_el1", tcr);
    
    // Set TTBR0 to point to our page table
    write_sysreg("ttbr0_el1", (uint64_t)pgd);
    
    // Identity map first 1GB (for kernel)
    arm64_mmu_map(0, 0, 1024 * 1024 * 1024, PTE_VALID | PTE_AF);
}

void arm64_mmu_enable(void) {
    // Enable MMU
    uint64_t sctlr = read_sysreg("sctlr_el1");
    sctlr |= SCTLR_EL1_M | SCTLR_EL1_C | SCTLR_EL1_I;
    write_sysreg("sctlr_el1", sctlr);
    
    // Ensure changes are visible
    __asm__ volatile("isb");
}

void arm64_mmu_map(uint64_t vaddr, uint64_t paddr, uint64_t size, uint64_t flags) {
    // Simplified mapping - maps 2MB blocks
    uint64_t addr = vaddr;
    uint64_t end = vaddr + size;
    
    while (addr < end) {
        uint64_t pgd_idx = (addr >> PGD_SHIFT) & 0x1FF;
        uint64_t pud_idx = (addr >> PUD_SHIFT) & 0x1FF;
        uint64_t pmd_idx = (addr >> PMD_SHIFT) & 0x1FF;
        
        // Set up PGD entry
        if (!(pgd[pgd_idx] & PTE_VALID)) {
            pgd[pgd_idx] = (uint64_t)pud | PTE_VALID | PTE_TABLE;
        }
        
        // Set up PUD entry
        if (!(pud[pud_idx] & PTE_VALID)) {
            pud[pud_idx] = (uint64_t)pmd | PTE_VALID | PTE_TABLE;
        }
        
        // Set up PMD entry (2MB block)
        uint64_t phys = paddr + (addr - vaddr);
        pmd[pmd_idx] = phys | flags | PTE_BLOCK;
        
        addr += (1UL << PMD_SHIFT);  // 2MB
    }
    
    // Flush TLB
    __asm__ volatile("tlbi vmalle1is");
    __asm__ volatile("dsb ish");
    __asm__ volatile("isb");
}

void arm64_mmu_unmap(uint64_t vaddr, uint64_t size) {
    // Simplified unmapping
    uint64_t addr = vaddr;
    uint64_t end = vaddr + size;
    
    while (addr < end) {
        uint64_t pmd_idx = (addr >> PMD_SHIFT) & 0x1FF;
        pmd[pmd_idx] = 0;
        addr += (1UL << PMD_SHIFT);
    }
    
    // Flush TLB
    __asm__ volatile("tlbi vmalle1is");
    __asm__ volatile("dsb ish");
    __asm__ volatile("isb");
}
