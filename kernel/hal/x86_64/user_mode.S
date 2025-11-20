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
    # rdi = entry_point
    # rsi = user_stack
    # rdx = rflags

    # Set the user data segment selector for DS, ES.
    # The 0x23 selector corresponds to the user data segment (GDT entry 4) with RPL=3.
    # We don't set SS here; `iretq` will set SS and CS atomically.
    mov $0x23, %ax
    mov %ax, %ds
    mov %ax, %es

    # It's good practice to zero out fs and gs when entering userspace,
    # unless they are specifically used for user-accessible data (like TLS).
    xor %eax, %eax
    mov %ax, %fs
    mov %ax, %gs

    # Prepare the iretq stack frame on the KERNEL stack
    # The order is critical: SS, RSP, RFLAGS, CS, RIP
    pushq $0x23     # SS: User Data Segment (RPL 3)
    pushq %rsi      # RSP: User Stack Pointer
    pushq %rdx      # RFLAGS: The flags for the user process
    pushq $0x1B     # CS: User Code Segment (RPL 3)
    pushq %rdi      # RIP: Instruction Pointer (entry point)

    # Zero out all general-purpose registers before entering user mode
    # to prevent leaking kernel data to the user process.
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

    # Atomically switch to Ring 3, load all the pushed registers,
    # and jump to the user-mode entry point.
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

