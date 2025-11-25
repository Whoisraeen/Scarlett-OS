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
    # Can be called from Limine (64-bit) or Multiboot2 (via our transition code)
    # RDI contains boot_info pointer (or NULL from Limine)

    # Early serial output - write 'K' to indicate kernel entry
    # Assuming bootloader initialized serial port
    mov $0x3F8, %dx
    mov $'K', %al
    out %al, %dx

    # Disable interrupts (just to be safe)
    cli

    # Save boot_info pointer (RDI) to R15 (callee-saved register)
    mov %rdi, %r15

    # Set up kernel stack (use RIP-relative addressing)
    lea stack_top(%rip), %rsp

    # Clear BSS section (but skip page tables at the start)
    # Page tables (pml4, pdpt, pd) are first 12KB of BSS - DON'T clear them!
    lea _bss_start(%rip), %rdi
    add $12288, %rdi              # Skip first 12KB (3x 4KB page tables)
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

    # Restore boot_info pointer to RDI (first argument to kernel_main)
    mov %r15, %rdi

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

