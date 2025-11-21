/**
 * @file vectors.S
 * @brief ARM64 exception vector table
 */

.section .text

.align 11
.global arm64_exception_vectors
arm64_exception_vectors:

// Current EL with SP0
.align 7
curr_el_sp0_sync:
    b exception_handler_sync

.align 7
curr_el_sp0_irq:
    b exception_handler_irq

.align 7
curr_el_sp0_fiq:
    b exception_handler_fiq

.align 7
curr_el_sp0_serror:
    b exception_handler_serror

// Current EL with SPx
.align 7
curr_el_spx_sync:
    b exception_handler_sync

.align 7
curr_el_spx_irq:
    b exception_handler_irq

.align 7
curr_el_spx_fiq:
    b exception_handler_fiq

.align 7
curr_el_spx_serror:
    b exception_handler_serror

// Lower EL using AArch64
.align 7
lower_el_aarch64_sync:
    b syscall_handler

.align 7
lower_el_aarch64_irq:
    b exception_handler_irq

.align 7
lower_el_aarch64_fiq:
    b exception_handler_fiq

.align 7
lower_el_aarch64_serror:
    b exception_handler_serror

// Lower EL using AArch32
.align 7
lower_el_aarch32_sync:
    b exception_handler_sync

.align 7
lower_el_aarch32_irq:
    b exception_handler_irq

.align 7
lower_el_aarch32_fiq:
    b exception_handler_fiq

.align 7
lower_el_aarch32_serror:
    b exception_handler_serror

// Exception handlers
exception_handler_sync:
    // TODO: Handle synchronous exceptions
    b .

exception_handler_irq:
    // Save context
    stp x29, x30, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x17, x18, [sp, #-16]!
    stp x15, x16, [sp, #-16]!
    stp x13, x14, [sp, #-16]!
    stp x11, x12, [sp, #-16]!
    stp x9, x10, [sp, #-16]!
    stp x7, x8, [sp, #-16]!
    stp x5, x6, [sp, #-16]!
    stp x3, x4, [sp, #-16]!
    stp x1, x2, [sp, #-16]!
    stp x0, x30, [sp, #-16]!

    // Call C IRQ handler
    bl arm64_irq_handler

    // Restore context
    ldp x0, x30, [sp], #16
    ldp x1, x2, [sp], #16
    ldp x3, x4, [sp], #16
    ldp x5, x6, [sp], #16
    ldp x7, x8, [sp], #16
    ldp x9, x10, [sp], #16
    ldp x11, x12, [sp], #16
    ldp x13, x14, [sp], #16
    ldp x15, x16, [sp], #16
    ldp x17, x18, [sp], #16
    ldp x19, x20, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x29, x30, [sp], #16

    eret

exception_handler_fiq:
    // TODO: Handle FIQ
    b .

exception_handler_serror:
    // TODO: Handle SError
    b .

syscall_handler:
    // Save user context
    stp x29, x30, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x17, x18, [sp, #-16]!
    stp x15, x16, [sp, #-16]!
    stp x13, x14, [sp, #-16]!
    stp x11, x12, [sp, #-16]!
    stp x9, x10, [sp, #-16]!
    stp x7, x8, [sp, #-16]!
    stp x5, x6, [sp, #-16]!
    stp x3, x4, [sp, #-16]!
    stp x1, x2, [sp, #-16]!
    str x0, [sp, #-16]!

    // Syscall number is in x8
    // Arguments are in x0-x7
    
    // Call C syscall handler
    mov x0, x8  // syscall number
    mov x1, sp  // pointer to saved registers
    bl arm64_syscall_handler

    // Restore user context
    ldr x0, [sp], #16
    ldp x1, x2, [sp], #16
    ldp x3, x4, [sp], #16
    ldp x5, x6, [sp], #16
    ldp x7, x8, [sp], #16
    ldp x9, x10, [sp], #16
    ldp x11, x12, [sp], #16
    ldp x13, x14, [sp], #16
    ldp x15, x16, [sp], #16
    ldp x17, x18, [sp], #16
    ldp x19, x20, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x29, x30, [sp], #16

    eret
