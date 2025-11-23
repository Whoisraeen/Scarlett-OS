/**
 * @file idt.c
 * @brief ARM64 interrupt handling setup
 * ARM64 uses exception vectors instead of IDT
 */

#include "../../include/types.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"

/**
 * Initialize ARM64 exception vector table
 */
error_code_t idt_init(void) {
    kinfo("Initializing ARM64 exception vectors...\n");
    
    // Set VBAR_EL1 (Vector Base Address Register) to our exception table
    // This is done in cpu_init.c, but we verify it here
    extern void arm64_exception_vectors(void);
    uint64_t vbar;
    __asm__ volatile("mrs %0, vbar_el1" : "=r"(vbar));
    
    if (vbar != (uint64_t)arm64_exception_vectors) {
        kwarn("VBAR_EL1 not set correctly, setting now...\n");
        __asm__ volatile("msr vbar_el1, %0" :: "r"((uint64_t)arm64_exception_vectors) : "memory");
        __asm__ volatile("isb");
    }
    
    // TODO: Set up exception handlers for: - DONE: Exception handlers are set up in vectors.S
    // - Synchronous exceptions (EL1) - handled by exception_handler_sync
    // - IRQ (Interrupt Request) - handled by exception_handler_irq
    // - FIQ (Fast Interrupt Request) - handled by exception_handler_fiq
    // - SError (System Error) - handled by exception_handler_serror
    // All handlers are implemented in vectors.S and exception_handler.c
    
    kinfo("ARM64 exception vectors initialized at 0x%016lx\n", (uint64_t)arm64_exception_vectors);
    
    return ERR_OK;
}
