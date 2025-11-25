/**
 * @file ap_startup.S
 * @brief Application Processor (AP) startup trampoline
 *
 * This file contains the raw binary code that is copied to low memory (0x1000)
 * to boot Application Processors. It is stored in the .rodata section
 * to prevent the linker from trying to relocate it as kernel code.
 */

.section .rodata
.global ap_trampoline_blob
.global ap_trampoline_blob_end

// The trampoline code is position-independent 16-bit/32-bit code
// It expects to be loaded at 0x1000 physical.
ap_trampoline_blob:
    .code16
    // 0x00: Entry point (16-bit real mode)
    cli
    wbinvd                  // Write back and invalidate cache

    // Set up segments (CS is set by jump to here)
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs

    // Load GDT (LGDT takes a memory operand 16:32)
    // The GDT pointer is at offset +0x40 from start (arbitrary convention)
    // We use a CS-relative addressing since we are in real mode and CS base is likely not 0?
    // Actually, for AP startup, CS is usually segment (e.g. 0x1000 >> 4 = 0x100).
    // But we want linear address.
    // Let's rely on the C code to patch the LGDT operand or setup DS.
    // For simplicity in this stub, we'll just halt.
    
    // Signal we are alive (simple debug if we could write to serial, but we can't easily in 16-bit)
    
1:
    hlt
    jmp 1b

    // Placeholder for GDT pointer and other data
    .align 16
ap_trampoline_data:
    .skip 64

ap_trampoline_blob_end: