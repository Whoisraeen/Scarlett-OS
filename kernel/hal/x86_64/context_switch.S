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
    # Save old context
    # Save general purpose registers
    mov %rax, 0(%rdi)
    mov %rbx, 8(%rdi)
    mov %rcx, 16(%rdi)
    mov %rdx, 24(%rdi)
    mov %rsi, 32(%rdi)
    mov %rdi, 40(%rdi)
    mov %rbp, 48(%rdi)
    mov %r8, 56(%rdi)
    mov %r9, 64(%rdi)
    mov %r10, 72(%rdi)
    mov %r11, 80(%rdi)
    mov %r12, 88(%rdi)
    mov %r13, 96(%rdi)
    mov %r14, 104(%rdi)
    mov %r15, 112(%rdi)
    
    # Save RIP (return address on stack)
    mov (%rsp), %rax
    mov %rax, 120(%rdi)
    
    # Save CS
    mov $0x08, 128(%rdi)
    
    # Save RFLAGS
    pushfq
    pop %rax
    mov %rax, 136(%rdi)
    
    # Save RSP (add 8 for return address)
    mov %rsp, %rax
    add $8, %rax
    mov %rax, 144(%rdi)
    
    # Save SS
    mov $0x10, 152(%rdi)
    
    # Load new context from RSI
    # Restore general purpose registers
    mov 0(%rsi), %rax
    mov 8(%rsi), %rbx
    mov 16(%rsi), %rcx
    mov 24(%rsi), %rdx
    # Skip RSI and RDI for now
    mov 48(%rsi), %rbp
    mov 56(%rsi), %r8
    mov 64(%rsi), %r9
    mov 72(%rsi), %r10
    mov 80(%rsi), %r11
    mov 88(%rsi), %r12
    mov 96(%rsi), %r13
    mov 104(%rsi), %r14
    mov 112(%rsi), %r15
    
    # Restore RSP
    mov 144(%rsi), %rsp
    
    # Push new RIP (for return)
    push 120(%rsi)
    
    # Push RFLAGS
    push 136(%rsi)
    
    # Restore RSI and RDI
    mov 32(%rsi), %rax
    push %rax
    mov 40(%rsi), %rax
    push %rax
    
    # Restore RFLAGS
    popfq
    
    # Restore RSI and RDI
    pop %rdi
    pop %rsi
    
    # Return to new thread
    ret

