/**
 * @file entry.S
 * @brief ARM64 kernel entry point
 */

.section .text
.global _start
.global kernel_entry

// ARM64 entry point
_start:
kernel_entry:
    // Set up stack pointer
    // Stack grows downward, so set to top of stack region
    ldr x0, =stack_top
    mov sp, x0
    
    // Clear BSS section
    ldr x0, =bss_start
    ldr x1, =bss_end
    mov x2, #0
    
clear_bss:
    cmp x0, x1
    b.ge bss_cleared
    str x2, [x0], #8
    b clear_bss
    
bss_cleared:
    // Jump to C kernel entry
    bl kernel_main
    
    // If kernel_main returns, halt
halt:
    wfi  // Wait for interrupt
    b halt

.section .bss
.align 16
stack_bottom:
    .space 0x10000  // 64KB stack
stack_top:

