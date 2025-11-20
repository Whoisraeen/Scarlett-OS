/**
 * @file entry.S
 * @brief Kernel entry point assembly stub
 *
 * This file contains the early kernel initialization code that runs
 * before the C kernel_main function is called.
 * Limine loads us directly in 64-bit long mode.
 */

.section .text
.global _start
.extern kernel_main

_start:
    # Limine loads us in 64-bit mode with:
    # - Paging enabled
    # - Interrupts disabled
    # - We're already in long mode

    # Disable interrupts (just to be safe)
    cli

    # Set up kernel stack
    lea stack_top(%rip), %rsp

    # Early serial output - write 'K' to indicate kernel entry
    mov $0x3F8, %dx          # COM1 base
    mov $'K', %al
    out %al, %dx

    # Clear BSS section
    lea _bss_start(%rip), %rdi
    lea _bss_end(%rip), %rcx
    sub %rdi, %rcx
    xor %rax, %rax
    rep stosb

    # Write 'B' to indicate BSS cleared
    mov $0x3F8, %dx
    mov $'B', %al
    out %al, %dx

    # Clear RFLAGS
    pushq $0
    popfq

    # Pass NULL as boot_info for now (Limine uses its own protocol)
    xor %rdi, %rdi

    # Write 'C' to indicate calling kernel_main
    mov $0x3F8, %dx
    mov $'C', %al
    out %al, %dx

    # Call kernel main
    call kernel_main

    # Write 'R' to indicate kernel_main returned
    mov $0x3F8, %dx
    mov $'R', %al
    out %al, %dx

    # If kernel_main returns, halt
    cli
1:
    hlt
    jmp 1b

.section .bss
.align 16
stack_bottom:
    .skip 65536  # 64KB kernel stack
stack_top:

