/**
 * @file entry.S
 * @brief RISC-V kernel entry point
 */

.section .text
.global _start
.global kernel_entry

// RISC-V entry point
// a0 = hartid
// a1 = dtb pointer
_start:
kernel_entry:
    // Save hartid to tp (Thread Pointer) register
    mv tp, a0
    
    // Save dtb pointer to s1 (callee-saved) for later use
    mv s1, a1

    // Set up stack pointer
    // Stack grows downward, so set to top of stack region
    la sp, stack_top
    
    // Clear BSS section
    la x0, bss_start
    la x1, bss_end
    li x2, 0
    
clear_bss:
    bge x0, x1, bss_cleared
    sd x2, 0(x0)
    addi x0, x0, 8
    j clear_bss
    
bss_cleared:
    // Pass dtb pointer to kernel_main (optional, but good practice)
    mv a0, s1
    
    // Jump to C kernel entry
    call kernel_main
    
    // If kernel_main returns, halt
halt:
    wfi  // Wait for interrupt
    j halt

.section .bss
.align 16
stack_bottom:
    .space 0x10000  // 64KB stack
stack_top: