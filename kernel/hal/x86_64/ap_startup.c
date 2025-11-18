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

// AP startup code location (4KB page)
#define AP_STARTUP_ADDR 0x1000

// AP startup code size
extern char ap_startup_16[];
extern char ap_startup_16_end[];

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
        // TODO: Set up kernel stack, idle stack, etc.
    }
    
    // Initialize scheduler for this CPU
    // TODO: Initialize per-CPU runqueue
    
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
    
    // Copy AP startup code to 0x1000
    size_t code_size = (size_t)(ap_startup_16_end - ap_startup_16);
    if (code_size > 4096) {
        kerror("AP startup code too large: %lu bytes\n", code_size);
        return ERR_INVALID_ARG;
    }
    
    // TODO: Copy startup code to 0x1000
    // For now, simplified
    
    // Send INIT IPI
    apic_send_init(apic_id);
    
    // Wait a bit
    for (volatile int i = 0; i < 10000; i++) {
        __asm__ volatile("pause");
    }
    
    // Send STARTUP IPI (vector 0x08 = entry point / 0x1000)
    apic_send_startup(apic_id, 0x08);
    
    // Wait a bit
    for (volatile int i = 0; i < 10000; i++) {
        __asm__ volatile("pause");
    }
    
    // Send second STARTUP IPI (some CPUs need two)
    apic_send_startup(apic_id, 0x08);
    
    kinfo("AP startup sequence sent for APIC ID %u\n", apic_id);
    
    return ERR_OK;
}

