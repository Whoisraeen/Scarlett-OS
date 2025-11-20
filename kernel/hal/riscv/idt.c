/**
 * @file idt.c
 * @brief RISC-V interrupt handling setup
 * RISC-V uses interrupt controllers and exception handlers
 */

#include "../../include/types.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"

/**
 * Initialize RISC-V interrupt handling
 */
error_code_t idt_init(void) {
    kinfo("Initializing RISC-V interrupt handling...\n");
    
    // Set up machine trap vector (mtvec)
    // This points to the exception/interrupt handler
    // TODO: Set mtvec register to exception handler address
    
    // Enable interrupts in machine status register (mstatus)
    // TODO: Set MIE (Machine Interrupt Enable) bit in mstatus
    
    // Enable external interrupts in machine interrupt enable (mie)
    // TODO: Enable MEIE (Machine External Interrupt Enable) in mie
    
    kinfo("RISC-V interrupt handling initialized\n");
    
    return ERR_OK;
}

