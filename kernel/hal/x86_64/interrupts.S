/**
 * @file interrupts.S
 * @brief Hardware interrupt handlers
 * 
 * Interrupt handlers for hardware interrupts (IRQ 0-15).
 * These are different from exceptions (0-31).
 */

.section .text

// External C handler
.extern interrupt_handler_c

/**
 * Common interrupt handler stub
 * Saves all registers and calls C handler
 */
.macro INTERRUPT_STUB num
.global interrupt_handler_\num
interrupt_handler_\num:
    // Save error code (0 for interrupts, but we push for consistency)
    push $0
    push $\num
    
    // Save all registers
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
    
    // Call C handler
    mov %rsp, %rdi  // Pass stack pointer as first argument
    call interrupt_handler_c
    
    // Check if we need to reschedule (for timer interrupt)
    // The C handler will have set a flag if needed
    // For now, we'll just return normally
    // TODO: Implement proper preemptive context switch from interrupt
    
    // Restore registers
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax
    
    // Remove interrupt number and error code
    add $16, %rsp
    
    // Return from interrupt
    iretq
.endm

// Generate interrupt handlers for IRQ 0-15 (interrupts 32-47)
INTERRUPT_STUB 32  // IRQ 0 - Timer
INTERRUPT_STUB 33  // IRQ 1 - Keyboard
INTERRUPT_STUB 34  // IRQ 2 - Cascade
INTERRUPT_STUB 35  // IRQ 3 - COM2
INTERRUPT_STUB 36  // IRQ 4 - COM1
INTERRUPT_STUB 37  // IRQ 5 - LPT2
INTERRUPT_STUB 38  // IRQ 6 - Floppy
INTERRUPT_STUB 39  // IRQ 7 - LPT1
INTERRUPT_STUB 40  // IRQ 8 - RTC
INTERRUPT_STUB 41  // IRQ 9 - Free
INTERRUPT_STUB 42  // IRQ 10 - Free
INTERRUPT_STUB 43  // IRQ 11 - Free
INTERRUPT_STUB 44  // IRQ 12 - PS/2 Mouse
INTERRUPT_STUB 45  // IRQ 13 - FPU
INTERRUPT_STUB 46  // IRQ 14 - Primary ATA
INTERRUPT_STUB 47  // IRQ 15 - Secondary ATA

