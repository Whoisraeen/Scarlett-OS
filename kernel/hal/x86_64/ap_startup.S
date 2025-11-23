/**
 * @file ap_startup.S
 * @brief Application Processor (AP) startup code
 *
 * This code runs on each AP when it's woken up.
 */

.section .ap_startup, "ax"
.code16

/**
 * AP startup code (16-bit, runs in real mode)
 * This is copied to a 4KB page at 0x1000
 */
.global ap_startup_16
.type ap_startup_16, @function
ap_startup_16:
    cli
    
    // Set up segment registers
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    
    // Load GDT
    lgdt ap_gdt_ptr - ap_startup_16
    
    // Enable protected mode
    mov %cr0, %eax
    or $1, %al
    mov %eax, %cr0
    
    // Jump to 32-bit code
    ljmp $0x08, $ap_startup_32 - ap_startup_16 + 0x1000

.code32
ap_startup_32:
    // Set up segment registers
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    
    // Enable PAE
    mov %cr4, %eax
    or $(1 << 5), %eax
    mov %eax, %cr4
    
    // Load page tables (shared with BSP)
    // All CPUs share the same kernel page tables initially
    // This will be patched by ap_startup() with the actual page table address
.global ap_page_table_addr
ap_page_table_addr:
    mov $0xDEADBEEF, %eax  // Placeholder - will be patched by ap_startup()
    mov %eax, %cr3
    
    // Enable long mode
    mov $0xC0000080, %ecx
    rdmsr
    or $(1 << 8), %eax
    wrmsr
    
    // Enable paging
    mov %cr0, %eax
    or $(1 << 31), %eax
    mov %eax, %cr0
    
    // Jump to 64-bit code
    ljmp $0x08, $ap_startup_64

.code64
ap_startup_64:
    // Set up 64-bit stack
    // Use temporary stack for now - will be switched to per-CPU stack in ap_init()
    // The per-CPU data is set up before AP startup, but we use temp stack initially
.global ap_stack_addr
ap_stack_addr:
    mov $0x200000, %rsp  // Temporary stack - ap_init() will switch to per-CPU stack
    
    // Call C initialization
    extern ap_init
    call ap_init
    
    // Should not return
    hlt

// GDT for AP startup
ap_gdt:
    .quad 0x0000000000000000  // Null descriptor
    .quad 0x00AF9A000000FFFF  // 64-bit code segment
    .quad 0x00CF92000000FFFF  // 64-bit data segment

ap_gdt_ptr:
    .word ap_gdt_ptr - ap_gdt - 1
    .quad ap_gdt

.global ap_startup_16_end
ap_startup_16_end:
