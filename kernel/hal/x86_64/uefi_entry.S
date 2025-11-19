/**
 * @file uefi_entry.S
 * @brief UEFI 64-bit entry point
 */

.section .text
.global uefi_entry
.extern kernel_main

.code64
uefi_entry:
    # We are already in 64-bit mode (Long Mode)
    # RDI contains the address of boot_info_t (System V ABI)
    
    # Disable interrupts
    cli
    
    # Set up kernel stack
    movabs $stack_top, %rsp
    
    # Clear RFLAGS
    pushq $0
    popfq
    
    # Call kernel_main(boot_info)
    # RDI is already set correctly
    callq kernel_main
    
    # Halt if return
.hang:
    cli
    hlt
    jmp .hang

.section .bss
.align 16
stack_bottom:
    .skip 65536 # 64KB stack
stack_top:
