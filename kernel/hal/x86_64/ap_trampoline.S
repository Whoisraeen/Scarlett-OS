/**
 * @file ap_trampoline.S
 * @brief Application Processor 16-bit startup trampoline
 *
 * This code is copied to low memory (0x8000) and executed by APs
 * when they receive a STARTUP IPI.
 */

.code16
.section .ap_trampoline, "ax"

.global ap_trampoline_start
.global ap_trampoline_end

ap_trampoline_start:
    cli

    # Clear segment registers
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    # Load GDT (using absolute address)
    lgdt (ap_gdt_desc - ap_trampoline_start + 0x8000)

    # Enable protected mode
    mov %cr0, %eax
    or $1, %al
    mov %eax, %cr0

    # Jump to 32-bit code (0x08 = code segment selector)
    ljmp $0x08, $(ap_protected_mode - ap_trampoline_start + 0x8000)

.code32
ap_protected_mode:
    # Set up 32-bit data segments
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    # Load CR3 with page table address (shared with BSP)
    # This will be patched by ap_startup() before copying
    mov $0xDEADBEEF, %eax    # Placeholder - will be patched
    mov %eax, %cr3

    # Enable PAE (Physical Address Extension)
    mov %cr4, %eax
    or $(1 << 5), %eax       # PAE bit
    mov %eax, %cr4

    # Enable long mode (set EFER.LME)
    mov $0xC0000080, %ecx    # EFER MSR
    rdmsr
    or $(1 << 8), %eax       # LME bit
    wrmsr

    # Enable paging and protected mode
    mov %cr0, %eax
    or $(1 << 31), %eax      # PG bit
    mov %eax, %cr0

    # Now in compatibility mode, jump to 64-bit code
    ljmp $0x08, $(ap_long_mode - ap_trampoline_start + 0x8000)

.code64
ap_long_mode:
    # Set up 64-bit segments
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    # Set up temporary stack (will be replaced in ap_init)
    # Stack address will be patched by ap_startup()
    movabs $0xDEADBEEFDEADBEEF, %rsp  # Placeholder - will be patched

    # Jump to C code (absolute address will be patched)
    movabs $0xDEADBEEFDEADBEEF, %rax  # Placeholder - will be patched
    call *%rax

    # Should never return, but if it does, halt
    cli
1:  hlt
    jmp 1b

# GDT for the trampoline
.align 16
ap_gdt:
    .quad 0x0000000000000000    # Null descriptor
    .quad 0x00AF9A000000FFFF    # 64-bit code segment (0x08)
    .quad 0x00CF92000000FFFF    # 64-bit data segment (0x10)

ap_gdt_desc:
    .word ap_gdt_desc - ap_gdt - 1
    .long (ap_gdt - ap_trampoline_start + 0x8000)

.align 4
ap_trampoline_end:
