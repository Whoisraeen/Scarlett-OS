/**
 * @file ap_startup.c
 * @brief Application Processor initialization
 */

#include "../../include/types.h"
#include "../../include/cpu.h"
#include "../../include/apic.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "../../include/mm/heap.h"
#include "../../include/mm/pmm.h"
#include "../../include/sched/scheduler.h"
#include "../../include/string.h"

// AP startup code location (below 1MB, 4KB aligned)
#define AP_STARTUP_ADDR 0x8000

// AP trampoline code
extern char ap_trampoline_start[];
extern char ap_trampoline_end[];

// Offsets for patching trampoline (must match ap_trampoline.S)
#define TRAMPOLINE_CR3_OFFSET    0x30   // Location of CR3 placeholder
#define TRAMPOLINE_STACK_OFFSET  0x5C   // Location of stack pointer placeholder
#define TRAMPOLINE_TARGET_OFFSET 0x66   // Location of ap_init address placeholder

/**
 * Initialize an Application Processor
 */
void ap_init(void) {
    uint32_t cpu_id = cpu_get_current_id();
    
    kinfo("AP %u: Initializing...\n", cpu_id);
    
    // Initialize Local APIC for this CPU
    apic_init();
    
    // Set up per-CPU data
    per_cpu_data_t* per_cpu = cpu_get_per_cpu_data(cpu_id);
    if (per_cpu) {
        per_cpu->cpu_id = cpu_id;
        
        // Allocate kernel stack (64KB)
        void* kernel_stack = kmalloc(64 * 1024);
        if (kernel_stack) {
            per_cpu->kernel_stack = kernel_stack;
        }
        
        // Allocate idle thread stack (64KB)
        void* idle_stack = kmalloc(64 * 1024);
        if (idle_stack) {
            per_cpu->idle_stack = idle_stack;
        }
        
        // Link CPU info
        cpu_info_t* info = cpu_get_info(cpu_id);
        if (info) {
            per_cpu->info = info;
            info->state = CPU_STATE_ONLINE;
        }
    }
    
    // Initialize scheduler for this CPU
    scheduler_init_per_cpu(cpu_id);
    
    kinfo("AP %u: Initialization complete\n", cpu_id);
    
    // Enter idle loop
    while (1) {
        __asm__ volatile("hlt");
    }
}

/**
 * Wake up an Application Processor
 */
error_code_t ap_startup(uint32_t apic_id) {
    kinfo("Starting AP with APIC ID %u...\n", apic_id);

    // Calculate trampoline size
    size_t trampoline_size = (size_t)(ap_trampoline_end - ap_trampoline_start);
    if (trampoline_size > 4096) {
        kerror("AP trampoline too large: %lu bytes\n", trampoline_size);
        return ERR_INVALID_ARG;
    }

    kinfo("Trampoline size: %lu bytes\n", trampoline_size);

    // Copy trampoline to low memory (0x8000)
    uint8_t* dest = (uint8_t*)AP_STARTUP_ADDR;
    const uint8_t* src = (const uint8_t*)ap_trampoline_start;

    for (size_t i = 0; i < trampoline_size; i++) {
        dest[i] = src[i];
    }

    // Patch CR3 with current page table
    uint64_t cr3_value;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3_value));
    *(uint32_t*)(dest + TRAMPOLINE_CR3_OFFSET) = (uint32_t)cr3_value;

    // Allocate stack for AP (64KB)
    void* ap_stack = kmalloc(64 * 1024);
    if (!ap_stack) {
        kerror("Failed to allocate AP stack\n");
        return ERR_OUT_OF_MEMORY;
    }

    // Patch stack pointer (point to top of stack)
    uint64_t stack_top = (uint64_t)ap_stack + (64 * 1024);
    *(uint64_t*)(dest + TRAMPOLINE_STACK_OFFSET) = stack_top;

    // Patch target address (ap_init function)
    uint64_t ap_init_addr = (uint64_t)&ap_init;
    *(uint64_t*)(dest + TRAMPOLINE_TARGET_OFFSET) = ap_init_addr;

    // Flush caches
    __asm__ volatile("mfence" ::: "memory");
    __asm__ volatile("wbinvd" ::: "memory");  // Write back and invalidate cache

    kinfo("Trampoline copied and patched at 0x%x\n", AP_STARTUP_ADDR);
    kinfo("  CR3: 0x%lx\n", cr3_value);
    kinfo("  Stack: 0x%lx\n", stack_top);
    kinfo("  Target: 0x%lx\n", ap_init_addr);

    // Send INIT IPI
    kinfo("Sending INIT IPI...\n");
    apic_send_init(apic_id);

    // Wait 10ms (as per Intel specification)
    for (volatile int i = 0; i < 100000; i++) {
        __asm__ volatile("pause");
    }

    // Send STARTUP IPI (vector = address / 4KB, so 0x8000 / 0x1000 = 0x08)
    uint8_t vector = AP_STARTUP_ADDR >> 12;
    kinfo("Sending STARTUP IPI (vector 0x%02x)...\n", vector);
    apic_send_startup(apic_id, vector);

    // Wait 200us
    for (volatile int i = 0; i < 2000; i++) {
        __asm__ volatile("pause");
    }

    // Send second STARTUP IPI (as per Intel manual)
    kinfo("Sending second STARTUP IPI...\n");
    apic_send_startup(apic_id, vector);

    // Wait for AP to initialize
    for (volatile int i = 0; i < 100000; i++) {
        __asm__ volatile("pause");
    }

    kinfo("AP startup sequence completed for APIC ID %u\n", apic_id);
    return ERR_OK;

    // Copy AP startup code to 0x1000
    // size_t code_size = (size_t)(ap_startup_16_end - ap_startup_16);
    // if (code_size > 4096) {
    //     kerror("AP startup code too large: %lu bytes\n", code_size);
    //     return ERR_INVALID_ARG;
    // }

    // Copy startup code to 0x1000
    // This address must be page-aligned and below 1MB
    // uint8_t* dest = (uint8_t*)AP_STARTUP_ADDR;
    // const uint8_t* src = (const uint8_t*)ap_startup_16;
    
    // // Copy code
    // for (size_t i = 0; i < code_size; i++) {
    //     dest[i] = src[i];
    // }

    // // Flush instruction cache
    // __asm__ volatile("mfence");

    // // Send INIT IPI
    // apic_send_init(apic_id);

    // // Wait a bit
    // for (volatile int i = 0; i < 10000; i++) {
    //     __asm__ volatile("pause");
    // }

    // // Send STARTUP IPI (vector 0x08 = entry point / 0x1000)
    // apic_send_startup(apic_id, 0x08);

    // // Wait a bit
    // for (volatile int i = 0; i < 10000; i++) {
    //     __asm__ volatile("pause");
    // }

    // // Send second STARTUP IPI (some CPUs need two)
    // apic_send_startup(apic_id, 0x08);

    // kinfo("AP startup sequence sent for APIC ID %u\n", apic_id);

    // return ERR_OK;
}

