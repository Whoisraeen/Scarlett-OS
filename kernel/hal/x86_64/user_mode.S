/**
 * @file user_mode.S
 * @brief User mode transition assembly code
 * 
 * Functions to switch from kernel mode (Ring 0) to user mode (Ring 3).
 */

.section .text

/**
 * Switch to user mode and jump to entry point
 * 
 * void enter_user_mode(vaddr_t entry_point, vaddr_t user_stack, uint64_t rflags);
 * 
 * This function:
 * 1. Sets up user stack
 * 2. Sets up segment registers for user mode
 * 3. Switches to Ring 3
 * 4. Jumps to user entry point
 */
.global enter_user_mode
enter_user_mode:
    # Function signature:
    # rdi = entry_point
    # rsi = user_stack
    # rdx = rflags
    
    # Save kernel stack pointer (we'll need it for syscalls)
    mov %rsp, %rax
    push %rax
    
    # Set up user stack
    mov %rsi, %rsp
    
    # Set up segment registers for user mode
    # User data segment (Ring 3): 0x23 (GDT entry 4, RPL 3)
    # User code segment (Ring 3): 0x1B (GDT entry 3, RPL 3)
    mov $0x23, %ax  # User data segment
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    
    # Push user stack segment and stack pointer (for iretq)
    push $0x23      # SS (user data segment)
    push %rsp       # RSP (user stack pointer)
    
    # Push RFLAGS
    push %rdx       # RFLAGS (passed as parameter)
    
    # Push user code segment and entry point (for iretq)
    push $0x1B      # CS (user code segment)
    push %rdi       # RIP (entry point)
    
    # Clear some registers (security)
    xor %rax, %rax
    xor %rbx, %rbx
    xor %rcx, %rcx
    xor %rdx, %rdx
    xor %rsi, %rsi
    xor %rdi, %rdi
    xor %rbp, %rbp
    xor %r8, %r8
    xor %r9, %r9
    xor %r10, %r10
    xor %r11, %r11
    xor %r12, %r12
    xor %r13, %r13
    xor %r14, %r14
    xor %r15, %r15
    
    # Use iretq to switch to user mode
    # This will pop: RIP, CS, RFLAGS, RSP, SS
    # and switch to Ring 3
    iretq

/**
 * Return to user mode after syscall
 * 
 * This is called after handling a syscall to return to user mode.
 * The user context should be on the stack.
 */
.global return_to_user_mode
return_to_user_mode:
    # Restore user segment registers
    mov $0x23, %ax  # User data segment
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
    # Return to user mode
    iretq

