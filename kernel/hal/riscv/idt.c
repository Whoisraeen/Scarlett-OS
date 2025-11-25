/**
 * @file idt.c
 * @brief RISC-V interrupt handling setup
 * 
 * Implements S-mode trap handling.
 */

#include "../../include/types.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"

// Supervisor Status Register bits
#define SSTATUS_SIE (1 << 1)  // Supervisor Interrupt Enable

// Supervisor Interrupt Enable Register bits
#define SIE_SSIE (1 << 1)  // Supervisor Software Interrupt Enable
#define SIE_STIE (1 << 5)  // Supervisor Timer Interrupt Enable
#define SIE_SEIE (1 << 9)  // Supervisor External Interrupt Enable

// Trap handler entry point (defined in assembly)
extern void riscv_trap_handler(void);

/**
 * Initialize RISC-V interrupt handling
 */
error_code_t idt_init(void) {
    kinfo("Initializing RISC-V interrupt handling (S-Mode)...");
    
    // Set stvec to point to our trap handler
    // Mode 1 (Vectored) or Mode 0 (Direct)
    // For simplicity, we use Direct mode (all traps go to same address)
    // Address must be 4-byte aligned
    uint64_t trap_entry = (uint64_t)riscv_trap_handler;
    __asm__ volatile("csrw stvec, %0" :: "r"(trap_entry & ~3ULL)); // Mode 0
    
    // Enable interrupts in sstatus (SIE)
    // Actually, we usually keep interrupts disabled during init and enable them later
    // via `hal_interrupts_enable`. 
    // But we should enable specific interrupt sources in `sie`.
    
    // Enable Supervisor Timer, Software, and External interrupts in SIE
    uint64_t sie;
    __asm__ volatile("csrr %0, sie" : "=r"(sie));
    sie |= (SIE_SSIE | SIE_STIE | SIE_SEIE);
    __asm__ volatile("csrw sie, %0" :: "r"(sie));
    
    kinfo("RISC-V interrupt handling initialized");
    
    return ERR_OK;
}