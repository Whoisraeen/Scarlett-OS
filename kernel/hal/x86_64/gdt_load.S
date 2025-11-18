/**
 * @file gdt.S
 * @brief GDT loading assembly routines
 */

.section .text
.global gdt_load

/**
 * Load GDT and reload segment registers
 * @param rdi - Pointer to GDT pointer structure
 */
gdt_load:
    # Load GDT
    lgdt (%rdi)
    
    # Reload code segment by doing a far return
    # Push new CS (0x08 = kernel code segment)
    pushq $0x08
    # Push return address
    lea .reload_cs(%rip), %rax
    pushq %rax
    # Far return to reload CS
    lretq
    
.reload_cs:
    # Reload data segments
    mov $0x10, %ax      # 0x10 = kernel data segment
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    ret

