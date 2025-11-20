/**
 * @file cpu.c
 * @brief CPU detection and management for RISC-V
 */

#include "../../include/types.h"
#include "../../include/cpu.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"

// CPU topology for RISC-V
static cpu_topology_t topology;
static per_cpu_data_t per_cpu_data[MAX_CPUS];

/**
 * Read Machine Information Register (mhartid)
 * Returns the hardware thread ID
 */
static uint64_t read_mhartid(void) {
    uint64_t hartid;
    __asm__ volatile("csrr %0, mhartid" : "=r"(hartid));
    return hartid;
}

/**
 * Read Machine Vendor ID (mvendorid)
 */
static uint32_t read_mvendorid(void) {
    uint32_t vendorid;
    __asm__ volatile("csrr %0, mvendorid" : "=r"(vendorid));
    return vendorid;
}

/**
 * Read Machine Architecture ID (marchid)
 */
static uint32_t read_marchid(void) {
    uint32_t archid;
    __asm__ volatile("csrr %0, marchid" : "=r"(archid));
    return archid;
}

/**
 * Read Machine Implementation ID (mimpid)
 */
static uint32_t read_mimpid(void) {
    uint32_t impid;
    __asm__ volatile("csrr %0, mimpid" : "=r"(impid));
    return impid;
}

/**
 * Detect number of CPUs (RISC-V)
 * This is platform-specific and may need device tree parsing
 */
static uint32_t detect_cpu_count(void) {
    // TODO: Parse device tree to get CPU count
    // For now, return 1 (BSP only)
    return 1;
}

/**
 * Initialize CPU information for BSP (RISC-V)
 */
static void init_bsp_cpu(void) {
    cpu_info_t* cpu = &topology.cpus[0];
    
    cpu->cpu_id = 0;
    cpu->apic_id = (uint32_t)read_mhartid();
    cpu->vendor_id = read_mvendorid();
    cpu->arch_id = read_marchid();
    cpu->impl_id = read_mimpid();
    
    // Set per-CPU data
    per_cpu_data[0].cpu_id = 0;
    per_cpu_data[0].is_bsp = true;
    
    kinfo("RISC-V CPU detected: Vendor=0x%08X, Arch=0x%08X, Impl=0x%08X\n",
          cpu->vendor_id, cpu->arch_id, cpu->impl_id);
}

/**
 * Initialize RISC-V CPU subsystem
 */
error_code_t cpu_init(void) {
    kinfo("Initializing RISC-V CPU subsystem...\n");
    
    // Initialize topology
    topology.cpu_count = detect_cpu_count();
    if (topology.cpu_count > MAX_CPUS) {
        topology.cpu_count = MAX_CPUS;
    }
    
    // Initialize BSP
    init_bsp_cpu();
    
    // TODO: Initialize other harts if SMP
    // This would involve parsing device tree for CPU nodes
    
    kinfo("RISC-V CPU subsystem initialized: %u CPU(s)\n", topology.cpu_count);
    
    return ERR_OK;
}

/**
 * Get current CPU ID (RISC-V)
 */
uint32_t cpu_get_id(void) {
    return (uint32_t)read_mhartid();
}

/**
 * Get CPU topology
 */
cpu_topology_t* cpu_get_topology(void) {
    return &topology;
}

/**
 * Get per-CPU data
 */
per_cpu_data_t* cpu_get_per_cpu_data(uint32_t cpu_id) {
    if (cpu_id >= MAX_CPUS) {
        return NULL;
    }
    return &per_cpu_data[cpu_id];
}

