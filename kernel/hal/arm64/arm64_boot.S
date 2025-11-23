/**
 * @file arm64_boot.S
 * @brief ARM64 Boot Assembly
 */

.section ".text.boot"

.global _start
_start:
    // Get CPU ID
    mrs x0, mpidr_el1
    and x0, x0, #0xFF
    cbz x0, primary_cpu
    
    // Secondary CPUs: wait for now
secondary_wait:
    wfe
    b secondary_wait

primary_cpu:
    // Set up stack
    ldr x0, =__stack_top
    mov sp, x0
    
    // Clear BSS
    ldr x0, =__bss_start
    ldr x1, =__bss_end
clear_bss:
    cmp x0, x1
    b.ge bss_done
    str xzr, [x0], #8
    b clear_bss

bss_done:
    // Jump to C code
    bl arm64_hal_early_init
    bl kernel_main
    
    // Should never reach here
halt:
    wfe
    b halt

// EL1 entry point (for EL2/EL3->EL1 transition)
.global el1_entry_point_label
el1_entry_point_label:
    // Re-initialize CPU at EL1
    // This is called after eret from EL2/EL3
    // Stack should already be set up
    bl arm64_cpu_init
    // Continue with normal initialization
    ret

.global arm64_context_switch
arm64_context_switch:
    // Save old context (x0 points to old_ctx)
    stp x0, x1, [x0, #0]
    stp x2, x3, [x0, #16]
    stp x4, x5, [x0, #32]
    stp x6, x7, [x0, #48]
    stp x8, x9, [x0, #64]
    stp x10, x11, [x0, #80]
    stp x12, x13, [x0, #96]
    stp x14, x15, [x0, #112]
    stp x16, x17, [x0, #128]
    stp x18, x19, [x0, #144]
    stp x20, x21, [x0, #160]
    stp x22, x23, [x0, #176]
    stp x24, x25, [x0, #192]
    stp x26, x27, [x0, #208]
    stp x28, x29, [x0, #224]
    str x30, [x0, #240]
    mov x2, sp
    str x2, [x0, #248]
    
    // Load new context (x1 points to new_ctx)
    ldp x0, x1, [x1, #0]
    ldp x2, x3, [x1, #16]
    ldp x4, x5, [x1, #32]
    ldp x6, x7, [x1, #48]
    ldp x8, x9, [x1, #64]
    ldp x10, x11, [x1, #80]
    ldp x12, x13, [x1, #96]
    ldp x14, x15, [x1, #112]
    ldp x16, x17, [x1, #128]
    ldp x18, x19, [x1, #144]
    ldp x20, x21, [x1, #160]
    ldp x22, x23, [x1, #176]
    ldp x24, x25, [x1, #192]
    ldp x26, x27, [x1, #208]
    ldp x28, x29, [x1, #224]
    ldr x30, [x1, #240]
    ldr x2, [x1, #248]
    mov sp, x2
    
    ret

.global arm64_irq_enable
arm64_irq_enable:
    msr daifclr, #2
    ret

.global arm64_irq_disable
arm64_irq_disable:
    msr daifset, #2
    ret
