/**
 * @file cpu.c
 * @brief CPU detection and management for ARM64
 */

#include "../../include/types.h"
#include "../../include/cpu.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"

// CPU topology for ARM64
static cpu_topology_t topology;
static per_cpu_data_t per_cpu_data[MAX_CPUS];

/**
 * Read CPU ID register (MPIDR_EL1)
 */
static uint64_t read_mpidr(void) {
    uint64_t mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return mpidr;
}

/**
 * Get CPU affinity from MPIDR
 */
static uint32_t get_cpu_affinity(void) {
    uint64_t mpidr = read_mpidr();
    return (uint32_t)(mpidr & 0xFF);  // Affinity level 0 (core ID)
}

/**
 * Get CPU cluster ID from MPIDR
 */
static uint32_t get_cluster_id(void) {
    uint64_t mpidr = read_mpidr();
    return (uint32_t)((mpidr >> 8) & 0xFF);  // Affinity level 1 (cluster ID)
}

/**
 * Read Main ID Register (MIDR_EL1) for CPU identification
 */
static uint64_t read_midr(void) {
    uint64_t midr;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(midr));
    return midr;
}

/**
 * Get CPU implementer from MIDR
 */
static uint32_t get_cpu_implementer(void) {
    uint64_t midr = read_midr();
    return (uint32_t)((midr >> 24) & 0xFF);
}

/**
 * Get CPU part number from MIDR
 */
static uint32_t get_cpu_part_number(void) {
    uint64_t midr = read_midr();
    return (uint32_t)((midr >> 4) & 0xFFF);
}

/**
 * Detect number of CPUs (ARM64)
 * This is platform-specific and may need device tree parsing
 */
static uint32_t detect_cpu_count(void) {
    // TODO: Parse device tree or ACPI to get CPU count - DONE: Device tree parsing implemented
    // Parse device tree to find CPU nodes
    extern dtb_node_t* dtb_get_root_node(void);
    extern dtb_node_t* dtb_find_node(const char* path);
    extern dtb_property_t* dtb_get_property(dtb_node_t* node, const char* name);
    
    dtb_node_t* root = dtb_get_root_node();
    if (!root) {
        kinfo("No device tree available, assuming 1 CPU\n");
        return 1;
    }
    
    // Find /cpus node
    dtb_node_t* cpus_node = dtb_find_node("/cpus");
    if (!cpus_node) {
        kinfo("No /cpus node found in device tree, assuming 1 CPU\n");
        return 1;
    }
    
    // Count CPU nodes (children of /cpus)
    uint32_t cpu_count = 0;
    dtb_node_t* child = cpus_node->child;
    while (child) {
        // Check if this is a CPU node (has "device_type" = "cpu" or "reg" property)
        dtb_property_t* device_type = dtb_get_property(child, "device_type");
        if (device_type) {
            const char* type = (const char*)device_type->data;
            if (strncmp(type, "cpu", 3) == 0) {
                cpu_count++;
            }
        } else {
            // If no device_type, check for reg property (CPUs usually have reg)
            dtb_property_t* reg = dtb_get_property(child, "reg");
            if (reg) {
                cpu_count++;
            }
        }
        child = child->sibling;
    }
    
    if (cpu_count == 0) {
        kinfo("No CPU nodes found in /cpus, assuming 1 CPU\n");
        return 1;
    }
    
    kinfo("Device tree reports %u CPU(s)\n", cpu_count);
    return cpu_count;
}

/**
 * Initialize CPU information for BSP (ARM64)
 */
static void init_bsp_cpu(void) {
    cpu_info_t* cpu = &topology.cpus[0];
    
    cpu->cpu_id = 0;
    cpu->apic_id = get_cpu_affinity();
    cpu->cluster_id = get_cluster_id();
    cpu->implementer = get_cpu_implementer();
    cpu->part_number = get_cpu_part_number();
    
    // Set per-CPU data
    per_cpu_data[0].cpu_id = 0;
    per_cpu_data[0].is_bsp = true;
    
    kinfo("ARM64 CPU detected: Implementer=0x%02X, Part=0x%03X\n",
          cpu->implementer, cpu->part_number);
}

/**
 * Initialize ARM64 CPU subsystem
 */
error_code_t cpu_init(void) {
    kinfo("Initializing ARM64 CPU subsystem...\n");
    
    // Initialize topology
    topology.cpu_count = detect_cpu_count();
    if (topology.cpu_count > MAX_CPUS) {
        topology.cpu_count = MAX_CPUS;
    }
    
    // Initialize BSP
    init_bsp_cpu();
    
    // TODO: Initialize APs (Application Processors) if SMP
    // This would involve parsing device tree for CPU nodes
    
    kinfo("ARM64 CPU subsystem initialized: %u CPU(s)\n", topology.cpu_count);
    
    return ERR_OK;
}

/**
 * Get current CPU ID (ARM64)
 */
uint32_t cpu_get_id(void) {
    uint64_t mpidr = read_mpidr();
    return (uint32_t)(mpidr & 0xFF);
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

