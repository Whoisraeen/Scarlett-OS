/**
 * @file multiboot2.S
 * @brief Multiboot2 header and entry point for GRUB booting
 *
 * GRUB loads us in 32-bit protected mode with:
 * - EAX = 0x36D76289 (Multiboot2 magic)
 * - EBX = physical address of Multiboot2 information structure
 *
 * We need to transition to 64-bit long mode and pass the info to kernel_main.
 */

.section .multiboot, "a"
.align 8

multiboot2_header_start:
    .long 0xE85250D6                    # Multiboot2 magic
    .long 0                             # Architecture (0 = i386)
    .long multiboot2_header_end - multiboot2_header_start  # Header length
    .long -(0xE85250D6 + 0 + (multiboot2_header_end - multiboot2_header_start))  # Checksum

    # Framebuffer tag
    .align 8
framebuffer_tag_start:
    .short 5                            # Type: framebuffer
    .short 0                            # Flags
    .long framebuffer_tag_end - framebuffer_tag_start  # Size
    .long 1024                          # Width
    .long 768                           # Height
    .long 32                            # Depth
framebuffer_tag_end:

    # End tag
    .align 8
    .short 0                            # Type: end
    .short 0                            # Flags
    .long 8                             # Size

multiboot2_header_end:

.section .text
.code32
.global multiboot2_entry
.extern _start

multiboot2_entry:
    # Save multiboot2 info (EAX = magic, EBX = info structure address)
    # We need to save EBX before it gets clobbered

    # Set up stack FIRST
    mov $stack_top, %esp

    # Now push EBX to our new stack
    push %ebx                           # Save Multiboot2 info address

    # Clear page tables (they're in BSS which is uninitialized)
    mov $pml4, %edi
    mov $12288, %ecx                    # 3 x 4KB = 12KB for pml4+pdpt+pd
    xor %eax, %eax
    rep stosb

    # Set up minimal page tables for identity mapping
    # PML4[0] -> PDPT (store address of pdpt in first PML4 entry)
    mov $pdpt, %eax
    or $3, %eax                         # Present + Writable
    mov $pml4, %edi
    mov %eax, (%edi)

    # PDPT[0] -> PD (store address of pd in first PDPT entry)
    mov $pd, %eax
    or $3, %eax
    mov $pdpt, %edi
    mov %eax, (%edi)

    # PD[0..3] -> 4x 2MB pages (identity map first 8MB)
    mov $pd, %edi
    mov $0x83, %eax                     # Present + Writable + Page Size (2MB)
    mov $4, %ecx
1:
    mov %eax, (%edi)
    add $0x200000, %eax                 # Next 2MB
    add $8, %edi
    loop 1b

    # Load page table
    mov $pml4, %eax
    mov %eax, %cr3

    # Enable PAE
    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4

    # Enable long mode (set LME bit in EFER MSR)
    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr

    # Enable paging
    mov %cr0, %eax
    or $0x80000000, %eax
    mov %eax, %cr0

    # Load 64-bit GDT
    lgdt gdt64_ptr

    # Far jump to 64-bit code segment
    ljmp $0x08, $long_mode_start

.code64
long_mode_start:
    # Now in 64-bit mode
    # Set up segment registers
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    # Retrieve Multiboot2 info address from stack (pushed in 32-bit mode)
    # The stack pointer was set up in 32-bit mode and preserved through mode switch
    # Pop the 32-bit value (zero-extended to 64-bit)
    pop %rdi                            # RDI = Multiboot2 info address (arg1 to kernel_main)

    # Jump to kernel entry
    jmp _start

# Minimal GDT for long mode
.align 16
gdt64:
    .quad 0                             # Null descriptor
    .quad 0x00AF9A000000FFFF            # 64-bit code segment
    .quad 0x00AF92000000FFFF            # 64-bit data segment
gdt64_ptr:
    .word gdt64_ptr - gdt64 - 1
    .quad gdt64

# Page tables (4KB aligned)
.section .bss
.align 4096
pml4:
    .skip 4096
pdpt:
    .skip 4096
pd:
    .skip 4096
stack_bottom:
    .skip 16384   # 16KB stack for setup
stack_top:
