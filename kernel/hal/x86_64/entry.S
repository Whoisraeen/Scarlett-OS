/**
 * @file entry.S
 * @brief Kernel entry point assembly stub
 * 
 * This file contains the early kernel initialization code that runs
 * before the C kernel_main function is called.
 */

.section .text
.global _start
.extern kernel_main

_start:
    # EAX contains Multiboot2 magic (0x36D76289)
    # EBX contains physical address of multiboot2 info structure
    # We're in 32-bit mode initially!
    
    # TODO: For now, just ignore multiboot2 info and pass NULL
    # A proper implementation would parse multiboot2 and create boot_info
    
    # Set up kernel stack
    lea stack_top(%rip), %rsp
    
    # Clear BSS section
    lea _bss_start(%rip), %rdi
    lea _bss_end(%rip), %rcx
    sub %rdi, %rcx
    xor %rax, %rax
    rep stosb
    
    # Clear RFLAGS
    pushq $0
    popfq
    
    # Pass NULL as boot_info for now (we'll fix this in Phase 2)
    xor %rdi, %rdi
    
    # Call kernel main
    call kernel_main
    
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

