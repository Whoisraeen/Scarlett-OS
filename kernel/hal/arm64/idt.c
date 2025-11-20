/**
 * @file idt.c
 * @brief ARM64 interrupt handling setup
 * ARM64 uses exception vectors instead of IDT
 */

#include "../../include/types.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"

// Exception vector table (set up in assembly)
extern void exception_vector_table(void);

/**
 * Initialize ARM64 exception vector table
 */
error_code_t idt_init(void) {
    kinfo("Initializing ARM64 exception vectors...\n");
    
    // Set VBAR_EL1 (Vector Base Address Register) to our exception table
    // This is done in assembly (entry.S or a separate file)
    
    // TODO: Set up exception handlers for:
    // - Synchronous exceptions (EL1)
    // - IRQ (Interrupt Request)
    // - FIQ (Fast Interrupt Request)
    // - SError (System Error)
    
    kinfo("ARM64 exception vectors initialized\n");
    
    return ERR_OK;
}

