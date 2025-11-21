/**
 * @file mmu.c
 * @brief ARM64 MMU and paging implementation
 */

#include "../../include/types.h"
#include "../../include/hal/hal.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"

// Page table entry bits
#define PTE_VALID       (1UL << 0)
#define PTE_TABLE       (1UL << 1)
#define PTE_PAGE        (1UL << 1)
#define PTE_BLOCK       (0UL << 1)
#define PTE_AF          (1UL << 10)  // Access flag
#define PTE_SH_INNER    (3UL << 8)   // Inner shareable
#define PTE_AP_RW       (0UL << 6)   // Read/write, EL1 only
#define PTE_AP_RW_USER  (1UL << 6)   // Read/write, EL0 and EL1
#define PTE_AP_RO       (2UL << 6)   // Read-only, EL1 only
#define PTE_AP_RO_USER  (3UL << 6)   // Read-only, EL0 and EL1
#define PTE_ATTR_IDX(x) ((x) << 2)   // Memory attribute index

// Memory attributes
#define MAIR_DEVICE_nGnRnE  0x00  // Device memory
#define MAIR_NORMAL_NC      0x44  // Normal memory, non-cacheable
#define MAIR_NORMAL         0xFF  // Normal memory, write-back cacheable

// TCR_EL1 bits
#define TCR_T0SZ(x)     ((x) << 0)
#define TCR_T1SZ(x)     ((x) << 16)
#define TCR_TG0_4KB     (0UL << 14)
#define TCR_TG1_4KB     (2UL << 30)
#define TCR_IPS_48BIT   (5UL << 32)
#define TCR_SH0_INNER   (3UL << 12)
#define TCR_SH1_INNER   (3UL << 28)
#define TCR_ORGN0_WBWA  (1UL << 10)
#define TCR_ORGN1_WBWA  (1UL << 26)
#define TCR_IRGN0_WBWA  (1UL << 8)
#define TCR_IRGN1_WBWA  (1UL << 24)

// Page sizes
#define PAGE_SIZE       4096
#define PAGE_SHIFT      12
#define ENTRIES_PER_TABLE 512

// Page table levels
typedef uint64_t pte_t;

// Kernel page tables (statically allocated for now)
static pte_t kernel_l0_table[ENTRIES_PER_TABLE] __attribute__((aligned(PAGE_SIZE)));
static pte_t kernel_l1_table[ENTRIES_PER_TABLE] __attribute__((aligned(PAGE_SIZE)));
static pte_t kernel_l2_table[ENTRIES_PER_TABLE] __attribute__((aligned(PAGE_SIZE)));
static pte_t kernel_l3_table[ENTRIES_PER_TABLE] __attribute__((aligned(PAGE_SIZE)));

static inline void write_ttbr0_el1(uint64_t val) {
    __asm__ volatile("msr ttbr0_el1, %0" :: "r"(val) : "memory");
}

static inline void write_ttbr1_el1(uint64_t val) {
    __asm__ volatile("msr ttbr1_el1, %0" :: "r"(val) : "memory");
}

static inline void write_tcr_el1(uint64_t val) {
    __asm__ volatile("msr tcr_el1, %0" :: "r"(val) : "memory");
}

static inline void write_mair_el1(uint64_t val) {
    __asm__ volatile("msr mair_el1, %0" :: "r"(val) : "memory");
}

static inline void write_sctlr_el1(uint64_t val) {
    __asm__ volatile("msr sctlr_el1, %0" :: "r"(val) : "memory");
}

static inline uint64_t read_sctlr_el1(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(val));
    return val;
}

error_code_t arm64_mmu_init(void) {
    kinfo("ARM64 MMU initialization...\n");

    // Clear page tables
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
        kernel_l0_table[i] = 0;
        kernel_l1_table[i] = 0;
        kernel_l2_table[i] = 0;
        kernel_l3_table[i] = 0;
    }

    // Set up identity mapping for first 1GB (for kernel)
    // L0[0] -> L1 table
    kernel_l0_table[0] = ((uint64_t)kernel_l1_table) | PTE_VALID | PTE_TABLE;

    // L1[0] -> L2 table (first 1GB)
    kernel_l1_table[0] = ((uint64_t)kernel_l2_table) | PTE_VALID | PTE_TABLE;

    // L2: Map first 512MB as 2MB blocks
    for (int i = 0; i < 256; i++) {
        uint64_t addr = (uint64_t)i << 21;  // 2MB blocks
        kernel_l2_table[i] = addr | PTE_VALID | PTE_BLOCK | PTE_AF | 
                            PTE_SH_INNER | PTE_AP_RW | PTE_ATTR_IDX(2);
    }

    // Set up higher-half mapping (0xFFFF_8000_0000_0000)
    // This requires L0[256] for TTBR1
    // For simplicity, we'll use the same mapping as identity for now

    // Configure MAIR_EL1 (Memory Attribute Indirection Register)
    uint64_t mair = (MAIR_DEVICE_nGnRnE << 0) |  // Index 0: Device
                    (MAIR_NORMAL_NC << 8) |       // Index 1: Normal non-cacheable
                    (MAIR_NORMAL << 16);          // Index 2: Normal cacheable
    write_mair_el1(mair);

    // Configure TCR_EL1 (Translation Control Register)
    uint64_t tcr = TCR_T0SZ(16) |      // 48-bit VA for TTBR0 (2^48 = 256TB)
                   TCR_T1SZ(16) |       // 48-bit VA for TTBR1
                   TCR_TG0_4KB |        // 4KB granule for TTBR0
                   TCR_TG1_4KB |        // 4KB granule for TTBR1
                   TCR_IPS_48BIT |      // 48-bit physical address
                   TCR_SH0_INNER |      // Inner shareable for TTBR0
                   TCR_SH1_INNER |      // Inner shareable for TTBR1
                   TCR_ORGN0_WBWA |     // Outer write-back write-allocate for TTBR0
                   TCR_ORGN1_WBWA |     // Outer write-back write-allocate for TTBR1
                   TCR_IRGN0_WBWA |     // Inner write-back write-allocate for TTBR0
                   TCR_IRGN1_WBWA;      // Inner write-back write-allocate for TTBR1
    write_tcr_el1(tcr);

    // Set TTBR0_EL1 (user-space page tables - same as kernel for now)
    write_ttbr0_el1((uint64_t)kernel_l0_table);

    // Set TTBR1_EL1 (kernel page tables)
    write_ttbr1_el1((uint64_t)kernel_l0_table);

    // Ensure all writes are complete
    __asm__ volatile("dsb sy");
    __asm__ volatile("isb");

    // Enable MMU
    uint64_t sctlr = read_sctlr_el1();
    sctlr |= (1 << 0);  // M bit - enable MMU
    sctlr |= (1 << 2);  // C bit - enable D-cache
    sctlr |= (1 << 12); // I bit - enable I-cache
    write_sctlr_el1(sctlr);

    __asm__ volatile("isb");

    kinfo("ARM64 MMU enabled\n");
    return ERR_OK;
}

void arm64_tlb_flush_all(void) {
    __asm__ volatile("tlbi vmalle1" ::: "memory");
    __asm__ volatile("dsb sy");
    __asm__ volatile("isb");
}

void arm64_tlb_flush_single(vaddr_t vaddr) {
    __asm__ volatile("tlbi vae1, %0" :: "r"(vaddr >> 12) : "memory");
    __asm__ volatile("dsb sy");
    __asm__ volatile("isb");
}
