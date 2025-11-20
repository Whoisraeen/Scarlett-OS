/**
 * @file entry.S
 * @brief ARM64 kernel entry point
 * 
 * This is the first code that runs when the kernel is loaded.
 * QEMU virt machine loads the kernel at 0x40080000.
 */

.section .text.start, "ax"
.global _start
.extern kernel_main
.extern _bss_start
.extern _bss_end

// ARM64 entry point
_start:
    // Disable interrupts
    msr daifset, #0xF  // Set all interrupt flags (disable interrupts)
    
    // Set up stack pointer
    // Stack grows downward, so set to top of stack region
    adrp x0, stack_top
    add x0, x0, :lo12:stack_top
    mov sp, x0
    
    // Early debug: Initialize UART and write 'K'
    // PL011 UART base address for QEMU virt machine
    mov x19, #0x9000000
    
    // Disable UART first
    mov w20, #0
    str w20, [x19, #0x30]  // UARTCR
    
    // Set baud rate (115200 @ 24MHz: divisor = 13.02)
    mov w20, #13
    str w20, [x19, #0x24]  // UARTIBRD
    mov w20, #1
    str w20, [x19, #0x28]  // UARTFBRD
    
    // 8 bits, no parity, 1 stop bit
    mov w20, #0x70
    str w20, [x19, #0x2C]  // UARTLCR_H
    
    // Enable UART, TX, RX
    mov w20, #0x301
    str w20, [x19, #0x30]  // UARTCR
    
    // Write 'K' to indicate kernel entry
    mov w20, #'K'
    str w20, [x19]  // UARTDR
    
    // Clear BSS section
    adrp x0, _bss_start
    add x0, x0, :lo12:_bss_start
    adrp x1, _bss_end
    add x1, x1, :lo12:_bss_end
    mov x2, #0
    
clear_bss:
    cmp x0, x1
    b.ge bss_cleared
    str x2, [x0], #8
    b clear_bss
    
bss_cleared:
    // Early debug: Write 'B' to UART (BSS cleared)
    mov w20, #'B'
    str w20, [x19]  // UARTDR
    
    // Set up argument for kernel_main (boot_info = NULL for ARM64)
    mov x0, #0  // boot_info_t* boot_info = NULL
    
    // Early debug: Write 'C' to UART (calling kernel_main)
    mov w20, #'C'
    str w20, [x19]  // UARTDR
    
    // Jump to C kernel entry
    bl kernel_main
    
    // If kernel_main returns, halt
    // Early debug: Write 'R' to UART (returned from kernel_main)
    mov w20, #'R'
    str w20, [x19]  // UARTDR
    
halt:
    wfi  // Wait for interrupt
    b halt

.section .bss
.align 16
stack_bottom:
    .space 0x10000  // 64KB stack
stack_top:

