/**
 * @file exceptions.S
 * @brief Exception handler stubs
 * 
 * These stubs save CPU state and call the C exception handler.
 */

.section .text
.extern exception_handler_c

# Macro to create exception handler without error code
.macro EXCEPTION_HANDLER num
.global exception_handler_\num
exception_handler_\num:
    push $0                 # Dummy error code
    push $\num              # Exception number
    jmp exception_common
.endm

# Macro to create exception handler with error code
.macro EXCEPTION_HANDLER_ERR num
.global exception_handler_\num
exception_handler_\num:
    push $\num              # Exception number (error code already pushed)
    jmp exception_common
.endm

# Exception handlers
EXCEPTION_HANDLER 0         # Divide Error
EXCEPTION_HANDLER 1         # Debug
EXCEPTION_HANDLER 2         # NMI
EXCEPTION_HANDLER 3         # Breakpoint
EXCEPTION_HANDLER 4         # Overflow
EXCEPTION_HANDLER 5         # Bound Range Exceeded
EXCEPTION_HANDLER 6         # Invalid Opcode
EXCEPTION_HANDLER 7         # Device Not Available
EXCEPTION_HANDLER_ERR 8     # Double Fault
EXCEPTION_HANDLER 9         # Coprocessor Segment Overrun
EXCEPTION_HANDLER_ERR 10    # Invalid TSS
EXCEPTION_HANDLER_ERR 11    # Segment Not Present
EXCEPTION_HANDLER_ERR 12    # Stack-Segment Fault
EXCEPTION_HANDLER_ERR 13    # General Protection Fault
EXCEPTION_HANDLER_ERR 14    # Page Fault
EXCEPTION_HANDLER 15        # Reserved
EXCEPTION_HANDLER 16        # x87 FPU Error
EXCEPTION_HANDLER_ERR 17    # Alignment Check
EXCEPTION_HANDLER 18        # Machine Check
EXCEPTION_HANDLER 19        # SIMD Floating-Point Exception
EXCEPTION_HANDLER 20        # Virtualization Exception
EXCEPTION_HANDLER_ERR 21    # Control Protection Exception
EXCEPTION_HANDLER 22        # Reserved
EXCEPTION_HANDLER 23        # Reserved
EXCEPTION_HANDLER 24        # Reserved
EXCEPTION_HANDLER 25        # Reserved
EXCEPTION_HANDLER 26        # Reserved
EXCEPTION_HANDLER 27        # Reserved
EXCEPTION_HANDLER 28        # Reserved
EXCEPTION_HANDLER 29        # Reserved
EXCEPTION_HANDLER 30        # Security Exception
EXCEPTION_HANDLER 31        # Reserved

# Common exception handler
exception_common:
    # Save all registers
    push %r15
    push %r14
    push %r13
    push %r12
    push %r11
    push %r10
    push %r9
    push %r8
    push %rbp
    push %rdi
    push %rsi
    push %rdx
    push %rcx
    push %rbx
    push %rax
    
    # Load kernel data segment
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    
    # Pass pointer to stack frame as argument
    mov %rsp, %rdi
    
    # Call C exception handler
    call exception_handler_c
    
    # Restore all registers
    pop %rax
    pop %rbx
    pop %rcx
    pop %rdx
    pop %rsi
    pop %rdi
    pop %rbp
    pop %r8
    pop %r9
    pop %r10
    pop %r11
    pop %r12
    pop %r13
    pop %r14
    pop %r15
    
    # Remove error code and exception number
    add $16, %rsp
    
    # Return from interrupt
    iretq

