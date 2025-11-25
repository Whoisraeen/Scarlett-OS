/**
 * @file hal_impl.c
 * @brief x86_64 HAL implementation
 */

#include "../../include/types.h"
#include "../../include/hal/hal.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "../../include/cpu.h"
#include "../../include/mm/vmm.h"

// Forward declarations for x86_64-specific functions
extern void interrupts_init(void);
extern void timer_init(void);
extern void syscall_init(void);
extern void serial_init(void);
extern error_code_t cpu_init(void);
extern error_code_t apic_init(void);
extern error_code_t ap_startup(uint32_t apic_id);
extern void context_switch(void* old_ctx, void* new_ctx);
extern void* get_boot_info(void);

// ============================================================================
// I/O Ports
// ============================================================================

void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// ============================================================================
// Architecture Detection
// ============================================================================

architecture_t hal_detect_architecture(void) {
    return HAL_ARCH_X86_64;
}

// ============================================================================
// CPU Management
// ============================================================================

error_code_t hal_cpu_init(void) {
    return cpu_init();
}

uint32_t hal_cpu_get_id(void) {
    extern uint32_t cpu_get_current_id(void);
    return cpu_get_current_id();
}

uint32_t hal_cpu_get_count(void) {
    extern uint32_t cpu_get_count(void);
    return cpu_get_count();
}

void hal_cpu_halt(void) {
    __asm__ volatile("hlt");
    while (1) {
        __asm__ volatile("hlt");
    }
}

void hal_interrupts_enable(void) {
    __asm__ volatile("sti");
}

void hal_interrupts_disable(void) {
    __asm__ volatile("cli");
}

bool hal_interrupts_enabled(void) {
    uint64_t rflags;
    __asm__ volatile("pushfq; pop %0" : "=r"(rflags));
    return (rflags & 0x200) != 0;  // IF flag
}

// ============================================================================
// Memory Management
// ============================================================================

error_code_t hal_mm_init(void) {
    extern void vmm_init(void);
    vmm_init();
    return ERR_OK;
}

void hal_tlb_flush_single(vaddr_t vaddr) {
    extern void vmm_flush_tlb_single(vaddr_t vaddr);
    vmm_flush_tlb_single(vaddr);
}

void hal_tlb_flush_all(void) {
    extern void vmm_flush_tlb_all(void);
    vmm_flush_tlb_all();
}

size_t hal_get_page_size(void) {
    return 4096;  // x86_64 uses 4KB pages
}

// ============================================================================
// Interrupts & Exceptions
// ============================================================================

error_code_t hal_interrupts_init(void) {
    interrupts_init();
    return ERR_OK;
}

error_code_t hal_irq_register(uint32_t irq, void (*handler)(void*), void* context) {
    extern error_code_t irq_register(uint8_t irq, void (*handler)(void*), void* context);
    return irq_register((uint8_t)irq, handler, context);
}

error_code_t hal_irq_unregister(uint32_t irq) {
    extern error_code_t irq_unregister(uint8_t irq);
    return irq_unregister((uint8_t)irq);
}

error_code_t hal_irq_enable(uint32_t irq) {
    extern void irq_enable(uint8_t irq);
    irq_enable((uint8_t)irq);
    return ERR_OK;
}

error_code_t hal_irq_disable(uint32_t irq) {
    extern void irq_disable(uint8_t irq);
    irq_disable((uint8_t)irq);
    return ERR_OK;
}

void hal_irq_eoi(uint32_t irq) {
    extern void apic_send_eoi(void);
    apic_send_eoi();
}

// ============================================================================
// Timers
// ============================================================================

error_code_t hal_timer_init(void) {
    timer_init();
    return ERR_OK;
}

uint64_t hal_timer_get_ticks(void) {
    extern uint64_t timer_get_ticks(void);
    return timer_get_ticks();
}

uint64_t hal_timer_get_frequency(void) {
    return 100;  // 100 Hz timer
}

error_code_t hal_timer_set_callback(void (*callback)(void)) {
    extern void timer_set_callback(void (*callback)(void));
    timer_set_callback(callback);
    return ERR_OK;
}

// ============================================================================
// System Calls
// ============================================================================

error_code_t hal_syscall_init(void) {
    syscall_init();
    return ERR_OK;
}

void hal_syscall_entry(void) {
    extern void syscall_entry(void);
    syscall_entry();
}

// ============================================================================
// Context Switching
// ============================================================================

// x86_64 CPU context (matches cpu_context_t from scheduler.h)
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} x86_64_context_t;

struct hal_cpu_context {
    x86_64_context_t ctx;
};

void hal_context_switch(hal_cpu_context_t* old_ctx, hal_cpu_context_t* new_ctx) {
    extern void context_switch(void* old_ctx, void* new_ctx);
    if (old_ctx && new_ctx) {
        context_switch(&old_ctx->ctx, &new_ctx->ctx);
    }
}

void hal_context_init(hal_cpu_context_t* ctx, vaddr_t stack_ptr, void (*entry)(void*), void* arg) {
    if (!ctx) return;
    
    x86_64_context_t* c = &ctx->ctx;
    
    // Initialize all registers to 0
    c->r15 = c->r14 = c->r13 = c->r12 = c->r11 = c->r10 = c->r9 = c->r8 = 0;
    c->rbp = c->rdi = c->rsi = c->rdx = c->rcx = c->rbx = c->rax = 0;
    
    // Set up stack pointer
    c->rsp = stack_ptr;
    c->ss = 0x18;  // User data segment
    
    // Set entry point
    c->rip = (uint64_t)entry;
    c->cs = 0x1B;  // User code segment (0x18 | 0x3)
    
    // Set flags (IF enabled, IOPL 0)
    c->rflags = 0x200;  // IF flag
    
    // First argument goes in RDI
    c->rdi = (uint64_t)arg;
}

// ============================================================================
// Atomic Operations
// ============================================================================

bool hal_atomic_cas(volatile uint64_t* ptr, uint64_t expected, uint64_t desired) {
    uint64_t prev;
    __asm__ volatile("lock cmpxchgq %2, %1"
                     : "=a"(prev), "+m"(*ptr)
                     : "r"(desired), "a"(expected)
                     : "memory");
    return prev == expected;
}

uint64_t hal_atomic_fetch_add(volatile uint64_t* ptr, uint64_t value) {
    uint64_t prev;
    __asm__ volatile("lock xaddq %0, %1"
                     : "=r"(prev), "+m"(*ptr)
                     : "0"(value)
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
        __asm__ volatile("clflush (%0)" :: "r"(p) : "memory");
        p += 64;
    }
    __asm__ volatile("mfence" ::: "memory");
}

void hal_cache_invalidate(void* addr, size_t size) {
    hal_cache_flush(addr, size);  // CLFLUSH invalidates too
}

void hal_cache_flush_invalidate(void* addr, size_t size) {
    hal_cache_flush(addr, size);
}

// ============================================================================
// Serial/Console
// ============================================================================

error_code_t hal_serial_init(void) {
    serial_init();
    return ERR_OK;
}

void hal_serial_write_char(char c) {
    extern void serial_write_char(char c);
    serial_write_char(c);
}

int hal_serial_read_char(void) {
    extern int serial_read_char(void);
    return serial_read_char();
}

// ============================================================================
// Boot & Initialization
// ============================================================================

error_code_t hal_early_init(void) {
    // GDT, IDT setup happens in entry.S and main.c
    return ERR_OK;
}

error_code_t hal_late_init(void) {
    // APIC initialization happens after VMM
    extern error_code_t apic_init(void);
    return apic_init();
}

void* hal_get_boot_info(void) {
    // Boot info is passed via RDI in entry.S
    // This is handled in main.c
    return NULL;  // Will be set up by entry code
}

// ============================================================================
// Multi-Core Support
// ============================================================================

error_code_t hal_ap_start(uint32_t cpu_id, vaddr_t entry_point) {
    (void)cpu_id;
    (void)entry_point;
    // extern error_code_t ap_startup(uint32_t apic_id);
    // For x86_64, we use APIC ID
    // return ap_startup(cpu_id);
    return ERR_NOT_SUPPORTED;
}

void* hal_get_per_cpu_data(uint32_t cpu_id) {
    extern per_cpu_data_t* cpu_get_per_cpu_data(uint32_t cpu_id);
    return (void*)cpu_get_per_cpu_data(cpu_id);
}

// ============================================================================
// Power Management
// ============================================================================

void hal_power_idle(void) {
    __asm__ volatile("hlt");
}

void hal_power_shutdown(void) {
    // ACPI shutdown (simplified)
    // Real implementation would use ACPI
    kerror("Shutdown not implemented\n");
    hal_cpu_halt();
}

void hal_power_reboot(void) {
    // Reboot via keyboard controller or ACPI
    kerror("Reboot not implemented\n");
    hal_cpu_halt();
}

