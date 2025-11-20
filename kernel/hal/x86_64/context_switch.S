/**
 * @file context_switch.S
 * @brief Context switching assembly code
 */

.section .text
.global context_switch

/**
 * Context switch between threads
 * @param rdi - Pointer to old context (cpu_context_t*)
 * @param rsi - Pointer to new context (cpu_context_t*)
 */
context_switch:
    # Save old context (RDI points to old context)
    # Save general purpose registers
    movq %rax, 0(%rdi)
    movq %rbx, 8(%rdi)
    movq %rcx, 16(%rdi)
    movq %rdx, 24(%rdi)
    movq %rsi, 32(%rdi)
    movq %rdi, 40(%rdi)
    movq %rbp, 48(%rdi)
    movq %r8, 56(%rdi)
    movq %r9, 64(%rdi)
    movq %r10, 72(%rdi)
    movq %r11, 80(%rdi)
    movq %r12, 88(%rdi)
    movq %r13, 96(%rdi)
    movq %r14, 104(%rdi)
    movq %r15, 112(%rdi)
    
    # Save RIP (return address on stack)
    movq (%rsp), %rax
    movq %rax, 120(%rdi)
    
    # Save CS
    movq $0x08, 128(%rdi)
    
    # Save RFLAGS
    pushfq
    popq %rax
    movq %rax, 136(%rdi)
    
    # Save RSP (add 8 for return address)
    movq %rsp, %rax
    addq $8, %rax
    movq %rax, 144(%rdi)
    
    # Save SS
    movq $0x10, 152(%rdi)
    
    # Load new context from RSI
    # Restore RSP first
    movq 144(%rsi), %rsp
    
    # Push new RIP (for return)
    pushq 120(%rsi)
    
    # Restore RFLAGS
    pushq 136(%rsi)
    popfq
    
    # Restore general purpose registers
    movq 0(%rsi), %rax
    movq 8(%rsi), %rbx
    movq 16(%rsi), %rcx
    movq 24(%rsi), %rdx
    movq 48(%rsi), %rbp
    movq 56(%rsi), %r8
    movq 64(%rsi), %r9
    movq 72(%rsi), %r10
    movq 80(%rsi), %r11
    movq 88(%rsi), %r12
    movq 96(%rsi), %r13
    movq 104(%rsi), %r14
    movq 112(%rsi), %r15
    
    # Restore RSI and RDI last
    movq 40(%rsi), %rdi
    movq 32(%rsi), %rsi
    
    # Return to new thread
    ret

