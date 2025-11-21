/**
 * @file context_switch.S
 * @brief ARM64 context switching
 */

.section .text

/**
 * void arm64_context_switch(hal_cpu_context_t* old_ctx, hal_cpu_context_t* new_ctx)
 * 
 * Context structure layout:
 * offset 0: x19-x28 (callee-saved registers)
 * offset 80: x29 (frame pointer)
 * offset 88: x30 (link register)
 * offset 96: sp (stack pointer)
 * offset 104: pc (program counter)
 * offset 112: spsr (saved program status register)
 */

.global arm64_context_switch
.type arm64_context_switch, %function
arm64_context_switch:
    // Save old context (x0 points to old_ctx)
    cbz x0, load_new_context  // Skip save if old_ctx is NULL

    // Save callee-saved registers
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    
    // Save frame pointer and link register
    stp x29, x30, [x0, #80]
    
    // Save stack pointer
    mov x9, sp
    str x9, [x0, #96]
    
    // Save program counter (return address)
    adr x9, 1f
    str x9, [x0, #104]
    
    // Save SPSR (if coming from user-space, otherwise use current)
    mrs x9, spsr_el1
    str x9, [x0, #112]

load_new_context:
    // Load new context (x1 points to new_ctx)
    
    // Load callee-saved registers
    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]
    
    // Load frame pointer and link register
    ldp x29, x30, [x1, #80]
    
    // Load stack pointer
    ldr x9, [x1, #96]
    mov sp, x9
    
    // Load program counter
    ldr x9, [x1, #104]
    
    // Load SPSR
    ldr x10, [x1, #112]
    msr spsr_el1, x10
    
    // Jump to new context
    br x9

1:
    // Return point after context switch
    ret

.size arm64_context_switch, . - arm64_context_switch

/**
 * void arm64_context_init_and_switch(hal_cpu_context_t* ctx, vaddr_t stack_ptr, 
 *                                     void (*entry)(void*), void* arg)
 * 
 * Initialize a new context and switch to it
 */
.global arm64_context_init_and_switch
.type arm64_context_init_and_switch, %function
arm64_context_init_and_switch:
    // x0 = ctx
    // x1 = stack_ptr
    // x2 = entry
    // x3 = arg

    // Clear callee-saved registers
    stp xzr, xzr, [x0, #0]
    stp xzr, xzr, [x0, #16]
    stp xzr, xzr, [x0, #32]
    stp xzr, xzr, [x0, #48]
    stp xzr, xzr, [x0, #64]
    stp xzr, xzr, [x0, #80]
    
    // Set stack pointer
    str x1, [x0, #96]
    
    // Set entry point as PC
    str x2, [x0, #104]
    
    // Set SPSR for user mode (EL0)
    // SPSR_EL1: M[3:0]=0000 (EL0t), all interrupts enabled
    mov x9, #0
    str x9, [x0, #112]
    
    // Store argument in x19 (will be moved to x0 in entry wrapper)
    str x3, [x0, #0]  // x19 position
    
    // Switch to new context
    mov x1, x0
    mov x0, xzr  // No old context to save
    b arm64_context_switch

.size arm64_context_init_and_switch, . - arm64_context_init_and_switch
