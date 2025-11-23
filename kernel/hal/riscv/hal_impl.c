/**
 * @file hal_impl.c
 * @brief RISC-V HAL implementation
 */

#include "../../include/types.h"
#include "../../include/hal/hal.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"

// ============================================================================
// Architecture Detection
// ============================================================================

architecture_t hal_detect_architecture(void) {
    return HAL_ARCH_RISCV;
}

// ============================================================================
// I/O Ports (RISC-V - No IO ports, MMIO only)
// ============================================================================

void outb(uint16_t port, uint8_t value) {
    (void)port;
    (void)value;
}

uint8_t inb(uint16_t port) {
    (void)port;
    return 0xFF;
}

// Stubs for other functions...
// ... (Simplified for brevity, focusing on unbreaking build)

error_code_t hal_cpu_init(void) { return ERR_OK; }
uint32_t hal_cpu_get_id(void) { return 0; }
uint32_t hal_cpu_get_count(void) { return 1; }
void hal_cpu_halt(void) { while(1); }
void hal_interrupts_enable(void) {}
void hal_interrupts_disable(void) {}
bool hal_interrupts_enabled(void) { return false; }
error_code_t hal_mm_init(void) { return ERR_OK; }
void hal_tlb_flush_single(vaddr_t vaddr) { (void)vaddr; }
void hal_tlb_flush_all(void) {}
size_t hal_get_page_size(void) { return 4096; }
error_code_t hal_interrupts_init(void) { return ERR_OK; }
error_code_t hal_irq_register(uint32_t irq, void (*handler)(void*), void* context) { (void)irq; (void)handler; (void)context; return ERR_OK; }
error_code_t hal_irq_unregister(uint32_t irq) { (void)irq; return ERR_OK; }
error_code_t hal_irq_enable(uint32_t irq) { (void)irq; return ERR_OK; }
error_code_t hal_irq_disable(uint32_t irq) { (void)irq; return ERR_OK; }
void hal_irq_eoi(uint32_t irq) { (void)irq; }
error_code_t hal_timer_init(void) { return ERR_OK; }
uint64_t hal_timer_get_ticks(void) { return 0; }
uint64_t hal_timer_get_frequency(void) { return 1000000; }
error_code_t hal_timer_set_callback(void (*callback)(void)) { (void)callback; return ERR_OK; }
error_code_t hal_syscall_init(void) { return ERR_OK; }
void hal_syscall_entry(void) {}
void hal_context_switch(hal_cpu_context_t* old_ctx, hal_cpu_context_t* new_ctx) { (void)old_ctx; (void)new_ctx; }
void hal_context_init(hal_cpu_context_t* ctx, vaddr_t stack_ptr, void (*entry)(void*), void* arg) { (void)ctx; (void)stack_ptr; (void)entry; (void)arg; }
bool hal_atomic_cas(volatile uint64_t* ptr, uint64_t expected, uint64_t desired) { (void)ptr; (void)expected; (void)desired; return false; }
uint64_t hal_atomic_fetch_add(volatile uint64_t* ptr, uint64_t value) { (void)ptr; (void)value; return 0; }
uint64_t hal_atomic_load(volatile uint64_t* ptr) { return *ptr; }
void hal_atomic_store(volatile uint64_t* ptr, uint64_t value) { *ptr = value; }
void hal_cache_flush(void* addr, size_t size) { (void)addr; (void)size; }
void hal_cache_invalidate(void* addr, size_t size) { (void)addr; (void)size; }
void hal_cache_flush_invalidate(void* addr, size_t size) { (void)addr; (void)size; }
error_code_t hal_serial_init(void) { return ERR_OK; }
void hal_serial_write_char(char c) { (void)c; }
int hal_serial_read_char(void) { return -1; }
error_code_t hal_early_init(void) { return ERR_OK; }
error_code_t hal_late_init(void) { return ERR_OK; }
void* hal_get_boot_info(void) { return NULL; }
error_code_t hal_ap_start(uint32_t cpu_id, vaddr_t entry_point) { (void)cpu_id; (void)entry_point; return ERR_OK; }
void* hal_get_per_cpu_data(uint32_t cpu_id) { (void)cpu_id; return NULL; }
void hal_power_idle(void) {}
void hal_power_shutdown(void) {}
void hal_power_reboot(void) {}
