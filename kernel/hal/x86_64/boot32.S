/**
 * @file boot32.S
 * @brief 32-bit to 64-bit transition for Multiboot2
 * 
 * Multiboot2 loads us in 32-bit protected mode.
 * We need to transition to 64-bit long mode.
 */



.section .boot, "ax"
.code32

.global _start
_start:
    # We're in 32-bit protected mode
    # EAX = Multiboot2 magic (0x36D76289)
    # EBX = Physical address of Multiboot2 info

    # Disable interrupts
    cli
    
    # Write 'S' to VGA as early debug marker (white on blue)
    movl $0xB8000, %edi
    movw $0x1F53, (%edi)  # 'S' white on blue

    # Set up temporary stack
    movl $boot_stack_top, %esp

    # Save Multiboot info pointer
    movl %ebx, temp_multiboot_ptr


    # Write 'C' for CPUID check
    movl $0xB8000, %edi
    movw $0x1F43, 2(%edi)  # 'C' white on blue
    
    # Check for long mode support
    call check_long_mode
    testl %eax, %eax
    jz .no_long_mode
    
    # Write 'P' for Page tables
    movl $0xB8000, %edi
    movw $0x1F50, 4(%edi)  # 'P' white on blue

    # Set up page tables
    call setup_page_tables

    # Enable PAE
    movl %cr4, %eax
    orl $(1 << 5), %eax
    movl %eax, %cr4

    # Set long mode bit in EFER MSR
    movl $0xC0000080, %ecx
    rdmsr
    orl $(1 << 8), %eax
    wrmsr

    # Load page table
    movl $boot_pml4, %eax
    movl %eax, %cr3

    # Enable paging
    movl %cr0, %eax
    orl $(1 << 31), %eax
    movl %eax, %cr0

    # Load 64-bit GDT
    lgdt boot_gdt64_pointer

    # Far jump to 64-bit code
    ljmp $0x08, $start64

.no_long_mode:
    cli
    hlt
    jmp .no_long_mode

# Check if CPU supports long mode
check_long_mode:
    # Check CPUID support
    pushfl
    popl %eax
    movl %eax, %ecx
    xorl $(1 << 21), %eax
    pushl %eax
    popfl
    pushfl
    popl %eax
    pushl %ecx
    popfl
    cmpl %ecx, %eax
    je .no_cpuid

    # Check extended functions
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb .no_long_mode_bit

    # Check long mode bit
    movl $0x80000001, %eax
    cpuid
    testl $(1 << 29), %edx
    jz .no_long_mode_bit

    movl $1, %eax
    ret

.no_cpuid:
.no_long_mode_bit:
    xorl %eax, %eax
    ret

# Set up identity-mapped page tables
setup_page_tables:
    # Clear page tables (3 pages = 12KB = 3072 dwords)
    movl $boot_pml4, %edi
    movl $3072, %ecx              # 3 pages * 4096 bytes / 4 bytes per dword
    xorl %eax, %eax
    rep stosl

    # PML4[0] -> PDPT
    movl $boot_pml4, %edi
    movl $boot_pdpt, %eax
    orl $0x03, %eax              # Present + Writable
    movl %eax, (%edi)

    # PDPT[0] -> PDT
    movl $boot_pdpt, %edi
    movl $boot_pdt, %eax
    orl $0x03, %eax
    movl %eax, (%edi)

    # PDT entries - identity map first 2GB with 2MB pages
    movl $boot_pdt, %edi
    movl $0x83, %eax             # Present + Writable + Huge (2MB)
    movl $512, %ecx
.fill_pdt:
    movl %eax, (%edi)
    addl $0x200000, %eax         # Next 2MB
    addl $8, %edi
    loop .fill_pdt

    ret

# 64-bit GDT for boot
.align 8
boot_gdt64:
    .quad 0x0000000000000000    # Null descriptor
    .quad 0x00209A0000000000    # 64-bit code segment
    .quad 0x0000920000000000    # 64-bit data segment

boot_gdt64_pointer:
    .word boot_gdt64_pointer - boot_gdt64 - 1
    .long boot_gdt64

# 64-bit entry point
.code64
start64:
    # Write '64' to VGA to show we're in 64-bit mode!
    movl $0xB8000, %edi
    movw $0x2F36, 6(%edi)  # '6' green on black
    movw $0x2F34, 8(%edi)  # '4' green on black
    
    # Set up segment registers
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    # Set up proper kernel stack
    movabs $stack_top, %rsp

    # Clear BSS
    movabs $_bss_start, %rdi
    movabs $_bss_end, %rcx
    subq %rdi, %rcx
    xorq %rax, %rax
    rep stosb

    # Clear RFLAGS
    pushq $0
    popfq

    # Load the saved multiboot pointer into RSI (second argument for C call)
    # movl is used to load the 32-bit address, which zero-extends to rsi.
    movl temp_multiboot_ptr, %esi

    # Set RDI to the address of our boot_info struct (first argument)
    movabs $boot_info, %rdi

    # Call the C parser
    .extern parse_multiboot_info
    callq parse_multiboot_info

    # Restore RDI for the call to kernel_main, as it's a volatile register
    movabs $boot_info, %rdi
    
    # Call kernel main
    .extern kernel_main
    callq kernel_main

    # Hang if kernel returns
    cli
.hang:
    hlt
    jmp .hang

# Boot-time page tables (in .boot section, low memory)
.section .boot.data, "aw"
.align 4096
boot_pml4:
    .skip 4096
boot_pdpt:
    .skip 4096
boot_pdt:
    .skip 4096

temp_multiboot_ptr:
    .long 0


# Boot stack (16KB)
.align 16
boot_stack_bottom:
    .skip 16384
boot_stack_top:

# Kernel stack (64KB, in high memory)
.section .bss
.align 16
boot_info:
    .skip 8192 # sizeof(boot_info_t) is mostly the memory map

.align 16
stack_bottom:
    .skip 65536
stack_top:
