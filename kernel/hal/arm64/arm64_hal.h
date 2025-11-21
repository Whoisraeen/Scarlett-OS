/**
 * @file arm64_hal.h
 * @brief ARM64 Hardware Abstraction Layer
 */

#ifndef KERNEL_HAL_ARM64_HAL_H
#define KERNEL_HAL_ARM64_HAL_H

#include <stdint.h>
#include <stdbool.h>

// Page size
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

// Memory attributes
#define MAIR_DEVICE_nGnRnE 0x00
#define MAIR_NORMAL_NC 0x44
#define MAIR_NORMAL 0xFF

// Page table entry flags
#define PTE_VALID (1UL << 0)
#define PTE_TABLE (1UL << 1)
#define PTE_PAGE (1UL << 1)
#define PTE_BLOCK (0UL << 1)
#define PTE_USER (1UL << 6)
#define PTE_RO (1UL << 7)
#define PTE_SHARED (3UL << 8)
#define PTE_AF (1UL << 10)
#define PTE_NG (1UL << 11)
#define PTE_PXN (1UL << 53)
#define PTE_UXN (1UL << 54)

// System registers
#define SCTLR_EL1_M (1 << 0)   // MMU enable
#define SCTLR_EL1_C (1 << 2)   // Cache enable
#define SCTLR_EL1_I (1 << 12)  // Instruction cache enable

// Exception levels
#define CURRENT_EL_EL0 0
#define CURRENT_EL_EL1 1
#define CURRENT_EL_EL2 2
#define CURRENT_EL_EL3 3

// CPU context for context switching
typedef struct {
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7;
    uint64_t x8, x9, x10, x11, x12, x13, x14, x15;
    uint64_t x16, x17, x18, x19, x20, x21, x22, x23;
    uint64_t x24, x25, x26, x27, x28, x29, x30;
    uint64_t sp, pc, pstate;
} arm64_context_t;

// HAL initialization
void arm64_hal_init(void);
void arm64_hal_early_init(void);

// MMU functions
void arm64_mmu_init(void);
void arm64_mmu_enable(void);
void arm64_mmu_map(uint64_t vaddr, uint64_t paddr, uint64_t size, uint64_t flags);
void arm64_mmu_unmap(uint64_t vaddr, uint64_t size);

// Cache operations
void arm64_cache_enable(void);
void arm64_cache_disable(void);
void arm64_cache_flush(void);
void arm64_cache_invalidate(void);

// Interrupt handling
void arm64_irq_init(void);
void arm64_irq_enable(void);
void arm64_irq_disable(void);
void arm64_irq_register(uint32_t irq, void (*handler)(void));

// Timer functions
void arm64_timer_init(void);
uint64_t arm64_timer_get_ticks(void);
void arm64_timer_set_interval(uint64_t interval_us);

// Context switching
void arm64_context_switch(arm64_context_t* old_ctx, arm64_context_t* new_ctx);
void arm64_context_init(arm64_context_t* ctx, void* entry, void* stack);

// CPU information
uint32_t arm64_get_cpu_id(void);
uint32_t arm64_get_num_cpus(void);
uint64_t arm64_get_current_el(void);

// Atomic operations
uint64_t arm64_atomic_add(uint64_t* ptr, uint64_t val);
uint64_t arm64_atomic_sub(uint64_t* ptr, uint64_t val);
bool arm64_atomic_cas(uint64_t* ptr, uint64_t old_val, uint64_t new_val);

#endif // KERNEL_HAL_ARM64_HAL_H
