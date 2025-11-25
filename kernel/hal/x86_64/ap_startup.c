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

// AP startup code location (4KB page)
#define AP_STARTUP_ADDR 0x1000

// AP startup code size
extern char ap_startup_16[];
extern char ap_startup_16_end[];

// Symbols in AP startup code that need patching
extern uint32_t ap_page_table_addr;
extern uint64_t ap_stack_addr;

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
error_code_t ap_initiate_startup(uint32_t apic_id) {
    kinfo("Starting AP with APIC ID %u...\n", apic_id);
    
    // Find CPU ID from APIC ID
    uint32_t cpu_id = 0;
    cpu_topology_t* topology = cpu_get_topology();
    for (uint32_t i = 0; i < topology->num_cpus; i++) {
        if (topology->cpus[i].apic_id == apic_id) {
            cpu_id = i;
            break;
        }
    }
    
    // Get per-CPU data for this CPU and set up stacks before AP starts
    per_cpu_data_t* per_cpu = cpu_get_per_cpu_data(cpu_id);
    if (!per_cpu) {
        kerror("AP startup: No per-CPU data for CPU %u\n", cpu_id);
        return ERR_INVALID_ARG;
    }
    
    // Allocate kernel stack if not already allocated
    if (!per_cpu->kernel_stack) {
        void* kernel_stack = kmalloc(64 * 1024);
        if (!kernel_stack) {
            kerror("AP startup: Failed to allocate kernel stack\n");
            return ERR_OUT_OF_MEMORY;
        }
        per_cpu->kernel_stack = kernel_stack;
    }
    
    // Allocate idle stack if not already allocated
    if (!per_cpu->idle_stack) {
        void* idle_stack = kmalloc(64 * 1024);
        if (!idle_stack) {
            kerror("AP startup: Failed to allocate idle stack\n");
            return ERR_OUT_OF_MEMORY;
        }
        per_cpu->idle_stack = idle_stack;
    }
    
    // Link CPU info
    cpu_info_t* info = cpu_get_info(cpu_id);
    if (info) {
        per_cpu->info = info;
    }
    
    // Get page table address (CR3) from BSP
    // All CPUs share the same kernel page tables initially
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint32_t cr3_low = (uint32_t)cr3;  // Lower 32 bits (physical address)
    
    // Copy AP startup code to 0x1000
    size_t code_size = (size_t)(ap_startup_16_end - ap_startup_16);
    if (code_size > 4096) {
        kerror("AP startup code too large: %lu bytes\n", code_size);
        return ERR_INVALID_ARG;
    }
    
    // Copy startup code to 0x1000
    // This address must be page-aligned and below 1MB
    uint8_t* dest = (uint8_t*)AP_STARTUP_ADDR;
    const uint8_t* src = (const uint8_t*)ap_startup_16;
    
    // Copy code
    for (size_t i = 0; i < code_size; i++) {
        dest[i] = src[i];
    }
    
    // Patch page table address (CR3) in the copied code
    // Find the placeholder at ap_page_table_addr offset
    // The instruction is: mov $0xDEADBEEF, %eax (opcode 0xB8 followed by 32-bit immediate)
    // Offset: ap_page_table_addr is the label, +1 byte for opcode, then 4 bytes for immediate
    uintptr_t page_table_offset = (uintptr_t)&ap_page_table_addr - (uintptr_t)ap_startup_16;
    uint32_t* page_table_patch = (uint32_t*)(dest + page_table_offset + 1);
    *page_table_patch = cr3_low;
    
    // Note: Stack will be set up in ap_init() after per-CPU data is initialized
    // The temporary stack (0x200000) is fine for now
    
    // Flush instruction cache
    __asm__ volatile("mfence");
    
    // Send INIT IPI
    apic_send_init(apic_id);
    
    // Wait a bit
    for (volatile int i = 0; i < 10000; i++) {
        __asm__ volatile("pause");
    }
    
    // Send STARTUP IPI (vector 0x01 = entry point 0x1000 / 0x1000)
    apic_send_startup(apic_id, 0x01);
    
    // Wait a bit
    for (volatile int i = 0; i < 10000; i++) {
        __asm__ volatile("pause");
    }
    
    // Send second STARTUP IPI (some CPUs need two)
    apic_send_startup(apic_id, 0x01);
    
    kinfo("AP startup sequence sent for APIC ID %u\n", apic_id);
    
    return ERR_OK;
}
