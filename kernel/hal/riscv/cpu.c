/**
 * @file cpu.c
 * @brief CPU detection and management for RISC-V
 */

#include "../../include/types.h"
#include "../../include/cpu.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "../../include/string.h"

// CPU topology for RISC-V
static cpu_topology_t topology;
static per_cpu_data_t per_cpu_data[MAX_CPUS];

// Basic DTB definitions (flattened device tree)
typedef struct {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
} fdt_header_t;

// External functions for DTB (assuming common/dtb.c or similar is linked)
// For now, declare mock functions if not available, or rely on what arm64 uses.
// Since cross-arch shared code structure isn't fully visible, I will inline a simple parser
// or reference external ones if the build system links them.
// Based on `kernel/hal/arm64/cpu.c`, it uses `dtb_get_root_node`.
// I'll assume these symbols are available globally or I'll define placeholders.

extern dtb_node_t* dtb_get_root_node(void);
extern dtb_node_t* dtb_find_node(const char* path);
extern dtb_property_t* dtb_get_property(dtb_node_t* node, const char* name);

/**
 * Read Hardware Thread ID (mhartid/or sscratch/tp convention in S-mode)
 * In S-mode, we can't read mhartid directly. 
 * Convention: `tp` register holds hartid or pointer to per-cpu data.
 * Bootloader usually passes hartid in a0.
 */
static uint64_t get_hartid(void) {
    uint64_t hartid;
    // Assuming we stored hartid in tp (Thread Pointer) during boot
    __asm__ volatile("mv %0, tp" : "=r"(hartid));
    return hartid;
}

/**
 * Detect number of CPUs (RISC-V)
 * This is platform-specific and requires parsing the Device Tree (DTB).
 */
static uint32_t detect_cpu_count(void) {
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
                // Further check node name
                if (strncmp(child->name, "cpu", 3) == 0) {
                    cpu_count++;
                }
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
 * Initialize CPU information for BSP (RISC-V)
 */
static void init_bsp_cpu(void) {
    cpu_info_t* cpu = &topology.cpus[0];
    
    cpu->cpu_id = 0;
    cpu->apic_id = 0; // Logical ID
    cpu->vendor_id = 0; // Cannot read mvendorid in S-mode
    cpu->arch_id = 0;
    cpu->impl_id = 0;
    
    // Set per-CPU data
    per_cpu_data[0].cpu_id = 0;
    per_cpu_data[0].is_bsp = true;
    
    kinfo("RISC-V CPU detected (Hart 0)\n");
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
    
    // Initialize other harts if SMP (via SBI HSM extension)
    // SBI (Supervisor Binary Interface) allows starting other harts.
    // For now, we only support BSP.
    
    kinfo("RISC-V CPU subsystem initialized: %u CPU(s)\n", topology.cpu_count);
    
    return ERR_OK;
}

/**
 * Get current CPU ID (RISC-V)
 */
uint32_t cpu_get_id(void) {
    // We assume `tp` holds the hartid or index.
    return (uint32_t)get_hartid();
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
