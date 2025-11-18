/**
 * @file idt.S
 * @brief IDT loading assembly routine
 */

.section .text
.global idt_load

/**
 * Load IDT
 * @param rdi - Pointer to IDT pointer structure
 */
idt_load:
    lidt (%rdi)
    ret

