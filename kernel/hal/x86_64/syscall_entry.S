/**
 * @file syscall_entry.S
 * @brief System call entry point
 */

.section .text
.global syscall_entry
.extern syscall_handler

/**
 * System call entry point
 * 
 * On entry:
 *   RAX = syscall number
 *   RDI = arg1
 *   RSI = arg2
 *   RDX = arg3
 *   R10 = arg4
 *   R8 = arg5
 *   R9 = arg6
 *   RCX = user RIP (saved by CPU)
 *   R11 = user RFLAGS (saved by CPU)
 */
syscall_entry:
    # Save user stack
    mov %rsp, %rbx
    
    # Switch to kernel stack
    mov thread_current_stack, %rsp
    
    # Save registers
    push %rbx      # user RSP
    push %r11      # user RFLAGS
    push %rcx      # user RIP
    push %r15
    push %r14
    push %r13
    push %r12
    push %rbp
    push %r9
    push %r8
    push %r10
    push %rdx
    push %rsi
    push %rdi
    push %rax
    
    # Call syscall handler
    # Arguments already in correct registers:
    # RDI (arg1), RSI (arg2), RDX (arg3), RCX (arg4), R8 (arg5), R9 (arg6)
    # But RAX needs to be first arg
    mov %rax, %rdi      # syscall_num
    mov %rdi, %rsi      # shift args
    mov %rsi, %rdx
    mov %rdx, %rcx
    mov %r10, %r8
    mov %r8, %r9
    
    call syscall_handler
    
    # Result in RAX
    mov %rax, %rbx
    
    # Restore registers
    pop %rax      # discard
    pop %rdi
    pop %rsi
    pop %rdx
    pop %r10
    pop %r8
    pop %r9
    pop %rbp
    pop %r12
    pop %r13
    pop %r14
    pop %r15
    pop %rcx      # user RIP
    pop %r11      # user RFLAGS
    pop %rsp      # user RSP
    
    # Move result to RAX
    mov %rbx, %rax
    
    # Return to user space
    sysretq

.data
.global thread_current_stack
thread_current_stack:
    .quad 0

