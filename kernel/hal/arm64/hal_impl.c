/**
 * @file hal_impl.c
 * @brief ARM64 HAL implementation (basic)
 */

#include "../../include/types.h"
#include "../../include/hal/hal.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "arm64_hal.h"
#include "dtb_parser.h"
#include "cpu.h"

// ============================================================================
// I/O Ports (ARM64 - No IO ports, MMIO only)
// ============================================================================

void outb(uint16_t port, uint8_t value) {
    (void)port;
    (void)value;
    // ARM64 uses MMIO, not I/O ports. This is a stub for compatibility.
    // Drivers should check HAL_ARCH before using port I/O.
}

uint8_t inb(uint16_t port) {
    (void)port;
    return 0xFF;
}

// ============================================================================
// Architecture Detection
// ============================================================================

architecture_t hal_detect_architecture(void) {
    return HAL_ARCH_ARM64;
}

// ============================================================================
// CPU Management
// ============================================================================

error_code_t hal_cpu_init(void) {
    extern error_code_t arm64_cpu_init(void);
    return arm64_cpu_init();
}

uint32_t hal_cpu_get_id(void) {
    extern uint32_t arm64_cpu_get_id(void);
    return arm64_cpu_get_id();
}

uint32_t hal_cpu_get_count(void) {
    }
    return cpus;
}

void hal_cpu_halt(void) {
    __asm__ volatile("wfi");  // Wait for interrupt
    while (1) {
        __asm__ volatile("wfi");
    }
}

void hal_interrupts_enable(void) {
    __asm__ volatile("msr daifclr, #2");  // Clear I bit (enable IRQ)
}

void hal_interrupts_disable(void) {
    __asm__ volatile("msr daifset, #2");  // Set I bit (disable IRQ)
}

bool hal_interrupts_enabled(void) {
    uint64_t daif;
    __asm__ volatile("mrs %0, daif" : "=r"(daif));
    return (daif & (1 << 7)) == 0;  // I bit clear = enabled
}
error_code_t hal_timer_init(void) {
    arm64_timer_init();
    return ERR_OK;
}

uint64_t hal_timer_get_ticks(void) {
    return arm64_timer_get_ticks();
}

uint64_t hal_timer_get_frequency(void) {
    uint64_t freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq;
}

error_code_t hal_timer_set_callback(void (*callback)(void)) {
    (void)callback;
    // Timer interrupt wiring is performed via arm64_timer_init for now.
    return ERR_OK;
}

// ============================================================================
// System Calls
// ============================================================================

error_code_t hal_syscall_init(void) {
    return ERR_OK;
}

void hal_syscall_entry(void) {
}

// ============================================================================
// Context Switching
// ============================================================================

// ARM64 CPU context layout matches arm64_context_switch stack frame
typedef struct {
    uint64_t x[31]; // x0..x30
    uint64_t sp;
} arm64_context_frame_t;

struct hal_cpu_context {
    arm64_context_frame_t ctx;
};

void hal_context_switch(hal_cpu_context_t* old_ctx, hal_cpu_context_t* new_ctx) {
    extern void arm64_context_switch(arm64_context_t*, arm64_context_t*);
    arm64_context_switch((arm64_context_t*)&old_ctx->ctx, (arm64_context_t*)&new_ctx->ctx);
}

void hal_context_init(hal_cpu_context_t* ctx, vaddr_t stack_ptr, void (*entry)(void*), void* arg) {
    if (!ctx) return;
    
    arm64_context_frame_t* c = &ctx->ctx;
    for (int i = 0; i < 31; i++) {
        c->x[i] = 0;
    }
    c->x[0] = (uint64_t)arg;          // First argument
    c->x[30] = (uint64_t)entry;       // Link register used by ret
    c->sp = stack_ptr;
}

// ============================================================================
// Atomic Operations
// ============================================================================

bool hal_atomic_cas(volatile uint64_t* ptr, uint64_t expected, uint64_t desired) {
    uint64_t result;
    __asm__ volatile("casal %0, %1, %2"
                     : "=r"(result), "+m"(*ptr)
                     : "r"(desired), "r"(expected)
                     : "memory");
    return result == expected;
}

uint64_t hal_atomic_fetch_add(volatile uint64_t* ptr, uint64_t value) {
    uint64_t prev;
    __asm__ volatile("ldaddal %0, %1, %2"
                     : "=r"(prev), "+m"(*ptr)
                     : "r"(value)
                     : "memory");
    return prev;
}

uint64_t hal_atomic_load(volatile uint64_t* ptr) {
    return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}

void hal_atomic_store(volatile uint64_t* ptr, uint64_t value) {
    __atomic_store_n(ptr, value, __ATOMIC_RELEASE);
}

// ============================================================================
// Cache Control
// ============================================================================

void hal_cache_flush(void* addr, size_t size) {
    uint8_t* p = (uint8_t*)addr;
    uint8_t* end = p + size;
    p = (uint8_t*)((uint64_t)p & ~63ULL);  // Align to cache line
    
    while (p < end) {
        __asm__ volatile("dc cvac, %0" :: "r"(p) : "memory");
        p += 64;
    }
    __asm__ volatile("dsb sy");
}

void hal_cache_invalidate(void* addr, size_t size) {
    uint8_t* p = (uint8_t*)addr;
    uint8_t* end = p + size;
    p = (uint8_t*)((uint64_t)p & ~63ULL);
    
    while (p < end) {
        __asm__ volatile("dc ivac, %0" :: "r"(p) : "memory");
        p += 64;
    }
    __asm__ volatile("dsb sy");
}

void hal_cache_flush_invalidate(void* addr, size_t size) {
    uint8_t* p = (uint8_t*)addr;
    uint8_t* end = p + size;
    p = (uint8_t*)((uint64_t)p & ~63ULL);
    
    while (p < end) {
        __asm__ volatile("dc civac, %0" :: "r"(p) : "memory");
        p += 64;
    }
    __asm__ volatile("dsb sy");
}

// ============================================================================
// Serial/Console
// ============================================================================

// Serial functions are implemented in hal/arm64/serial.c
// These are just wrappers to maintain HAL interface consistency
extern error_code_t hal_serial_init(void);
extern void hal_serial_write_char(char c);
extern int hal_serial_read_char(void);

// ============================================================================
// Boot & Initialization
// ============================================================================

error_code_t hal_early_init(void) {
    return ERR_OK;
}

error_code_t hal_late_init(void) {
    return ERR_OK;
}

void* hal_get_boot_info(void) {
    // Return device tree root node as boot info
    return (void*)dtb_get_root_node();
}

// ============================================================================
// Multi-Core Support
// ============================================================================

error_code_t hal_ap_start(uint32_t cpu_id, vaddr_t entry_point) {
    // Start secondary CPU via PSCI CPU_ON
    // PSCI function ID: 0x84000003 (CPU_ON)
    // Arguments: x0 = function_id, x1 = target_cpu, x2 = entry_point, x3 = context_id
    uint64_t function_id = 0x84000003;  // PSCI CPU_ON
    uint64_t target_cpu = cpu_id;
    uint64_t entry = (uint64_t)entry_point;
    uint64_t context_id = 0;  // Context ID (not used)
    
    uint64_t result;
    __asm__ volatile(
        "mov x0, %1\n"
        "mov x1, %2\n"
        "mov x2, %3\n"
        "mov x3, %4\n"
        "smc #0\n"
        "mov %0, x0"
        : "=r"(result)
        : "r"(function_id), "r"(target_cpu), "r"(entry), "r"(context_id)
        : "x0", "x1", "x2", "x3", "memory"
    );
    
    // PSCI_SUCCESS = 0
    if (result == 0) {
        return ERR_OK;
    } else {
        // PSCI error codes: -1 = NOT_SUPPORTED, -2 = INVALID_PARAMETERS, etc.
        return ERR_NOT_SUPPORTED;
    }
}

void* hal_get_per_cpu_data(uint32_t cpu_id) {
    // Get per-CPU data from CPU subsystem
    extern per_cpu_data_t* cpu_get_per_cpu_data(uint32_t cpu_id);
    per_cpu_data_t* per_cpu = cpu_get_per_cpu_data(cpu_id);
    return (void*)per_cpu;
}

// ============================================================================
// Power Management
// ============================================================================

void hal_power_idle(void) {
    __asm__ volatile("wfi");  // Wait for interrupt
}

void hal_power_shutdown(void) {
    // Use PSCI SYSTEM_OFF to shutdown
    // PSCI function ID: 0x84000008 (SYSTEM_OFF)
    // This uses SMC (Secure Monitor Call) or HVC (Hypervisor Call) depending on EL
    uint64_t function_id = 0x84000008;  // PSCI SYSTEM_OFF
    __asm__ volatile(
        "mov x0, %0\n"
        "smc #0"
        :
        : "r"(function_id)
        : "x0", "memory"
    );
    // If SMC returns, fall back to halt
    hal_cpu_halt();
}

void hal_power_reboot(void) {
    // Use PSCI SYSTEM_RESET to reboot
    // PSCI function ID: 0x84000009 (SYSTEM_RESET)
    uint64_t function_id = 0x84000009;  // PSCI SYSTEM_RESET
    __asm__ volatile(
        "mov x0, %0\n"
        "smc #0"
        :
        : "r"(function_id)
        : "x0", "memory"
    );
    // If SMC returns, fall back to halt
    hal_cpu_halt();
}

