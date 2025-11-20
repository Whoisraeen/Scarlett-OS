/**
 * @file hal.h
 * @brief Hardware Abstraction Layer (HAL) Interface
 * 
 * This file defines the architecture-independent HAL interface.
 * Each architecture (x86_64, ARM64, RISC-V) must implement these functions.
 */

#ifndef KERNEL_HAL_HAL_H
#define KERNEL_HAL_HAL_H

#include "../types.h"
#include "../errors.h"

// Architecture types
typedef enum {
    ARCH_X86_64,
    ARCH_ARM64,
    ARCH_RISCV,
    ARCH_UNKNOWN
} architecture_t;

// ============================================================================
// Architecture Detection
// ============================================================================

/**
 * Detect current architecture
 */
architecture_t hal_detect_architecture(void);

/**
 * Get architecture name string
 */
const char* hal_get_architecture_name(architecture_t arch);

// ============================================================================
// CPU Management
// ============================================================================

/**
 * Initialize CPU subsystem
 */
error_code_t hal_cpu_init(void);

/**
 * Get current CPU ID
 */
uint32_t hal_cpu_get_id(void);

/**
 * Get CPU count
 */
uint32_t hal_cpu_get_count(void);

/**
 * Halt CPU (infinite loop)
 */
void hal_cpu_halt(void);

/**
 * Enable interrupts
 */
void hal_interrupts_enable(void);

/**
 * Disable interrupts
 */
void hal_interrupts_disable(void);

/**
 * Check if interrupts are enabled
 */
bool hal_interrupts_enabled(void);

// ============================================================================
// Memory Management
// ============================================================================

/**
 * Initialize paging structures
 */
error_code_t hal_mm_init(void);

/**
 * Flush TLB for a single address
 */
void hal_tlb_flush_single(vaddr_t vaddr);

/**
 * Flush entire TLB
 */
void hal_tlb_flush_all(void);

/**
 * Get page size
 */
size_t hal_get_page_size(void);

// ============================================================================
// Interrupts & Exceptions
// ============================================================================

/**
 * Initialize interrupt subsystem
 */
error_code_t hal_interrupts_init(void);

/**
 * Register interrupt handler
 * @param irq Interrupt number
 * @param handler Handler function
 * @param context Context pointer
 */
error_code_t hal_irq_register(uint32_t irq, void (*handler)(void*), void* context);

/**
 * Unregister interrupt handler
 */
error_code_t hal_irq_unregister(uint32_t irq);

/**
 * Enable specific IRQ
 */
error_code_t hal_irq_enable(uint32_t irq);

/**
 * Disable specific IRQ
 */
error_code_t hal_irq_disable(uint32_t irq);

/**
 * Send end-of-interrupt (EOI)
 */
void hal_irq_eoi(uint32_t irq);

// ============================================================================
// Timers
// ============================================================================

/**
 * Initialize timer subsystem
 */
error_code_t hal_timer_init(void);

/**
 * Get timer ticks (monotonic counter)
 */
uint64_t hal_timer_get_ticks(void);

/**
 * Get timer frequency (Hz)
 */
uint64_t hal_timer_get_frequency(void);

/**
 * Set timer callback (for scheduler ticks)
 */
error_code_t hal_timer_set_callback(void (*callback)(void));

// ============================================================================
// System Calls
// ============================================================================

/**
 * Initialize system call interface
 */
error_code_t hal_syscall_init(void);

/**
 * System call entry point (called from userspace)
 */
void hal_syscall_entry(void);

// ============================================================================
// Context Switching
// ============================================================================

/**
 * CPU context structure (architecture-specific size)
 */
typedef struct hal_cpu_context hal_cpu_context_t;

/**
 * Switch context (save old, restore new)
 */
void hal_context_switch(hal_cpu_context_t* old_ctx, hal_cpu_context_t* new_ctx);

/**
 * Initialize context for new thread
 */
void hal_context_init(hal_cpu_context_t* ctx, vaddr_t stack_ptr, void (*entry)(void*), void* arg);

// ============================================================================
// Atomic Operations
// ============================================================================

/**
 * Atomic compare and swap
 */
bool hal_atomic_cas(volatile uint64_t* ptr, uint64_t expected, uint64_t desired);

/**
 * Atomic fetch and add
 */
uint64_t hal_atomic_fetch_add(volatile uint64_t* ptr, uint64_t value);

/**
 * Atomic load
 */
uint64_t hal_atomic_load(volatile uint64_t* ptr);

/**
 * Atomic store
 */
void hal_atomic_store(volatile uint64_t* ptr, uint64_t value);

// ============================================================================
// Cache Control
// ============================================================================

/**
 * Flush data cache
 */
void hal_cache_flush(void* addr, size_t size);

/**
 * Invalidate data cache
 */
void hal_cache_invalidate(void* addr, size_t size);

/**
 * Flush and invalidate data cache
 */
void hal_cache_flush_invalidate(void* addr, size_t size);

// ============================================================================
// Serial/Console
// ============================================================================

/**
 * Initialize serial port
 */
error_code_t hal_serial_init(void);

/**
 * Write character to serial port
 */
void hal_serial_write_char(char c);

/**
 * Read character from serial port (non-blocking)
 * @return Character or -1 if no data available
 */
int hal_serial_read_char(void);

// ============================================================================
// Boot & Initialization
// ============================================================================

/**
 * Early initialization (before heap is ready)
 */
error_code_t hal_early_init(void);

/**
 * Late initialization (after heap is ready)
 */
error_code_t hal_late_init(void);

/**
 * Get boot information structure
 */
void* hal_get_boot_info(void);

// ============================================================================
// Multi-Core Support
// ============================================================================

/**
 * Start application processor
 * @param cpu_id CPU ID to start
 * @param entry_point Entry point address
 * @return 0 on success, -1 on error
 */
error_code_t hal_ap_start(uint32_t cpu_id, vaddr_t entry_point);

/**
 * Get per-CPU data pointer
 */
void* hal_get_per_cpu_data(uint32_t cpu_id);

// ============================================================================
// Power Management
// ============================================================================

/**
 * Enter low-power state
 */
void hal_power_idle(void);

/**
 * Shutdown system
 */
void hal_power_shutdown(void);

/**
 * Reboot system
 */
void hal_power_reboot(void);

#endif // KERNEL_HAL_HAL_H

