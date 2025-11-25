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
    // DONE: Device tree parsing implemented
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
// PSCI Function IDs (SMC/HVC)
#define PSCI_0_2_FN_CPU_ON          0xC4000003
#define PSCI_0_2_FN_CPU_OFF         0x84000002
#define PSCI_0_2_FN_SYSTEM_RESET    0x84000009

// PSCI Return Values
#define PSCI_SUCCESS        0
#define PSCI_NOT_SUPPORTED  -1
#define PSCI_INVALID_PARAMS -2
#define PSCI_DENIED         -3
#define PSCI_ALREADY_ON     -4
#define PSCI_ON_PENDING     -5
#define PSCI_INTERNAL_FAILURE -6
#define PSCI_NOT_PRESENT    -7
#define PSCI_DISABLED       -8

// External assembly function for secondary CPU startup
extern void secondary_startup(void);

// Secondary CPU C entry point
void secondary_cpu_entry(void) {
    uint32_t cpu_id = get_cpu_affinity(); // Simplified, should map MPIDR to logical ID
    
    // Setup per-CPU data
    // Note: This is a simplified mapping. Real OS needs a proper logical ID allocator.
    // For now, we assume cpu_id matches the index in per_cpu_data if small enough.
    if (cpu_id < MAX_CPUS) {
        per_cpu_data[cpu_id].cpu_id = cpu_id;
        per_cpu_data[cpu_id].is_bsp = false;
        per_cpu_data[cpu_id].current_thread = NULL;
    }
    
    // Enable MMU (using same page tables as BSP)
    extern void arm64_mmu_enable(void);
    extern void arm64_mmu_init(void); // Should not re-init, just load TTBR0?
    // Actually, TTBR0 is shared if we use the same page tables.
    // We just need to enable MMU.
    // But we need to set TTBR0 first.
    extern uint64_t boot_pgd[]; // From arm64_mmu.c (needs to be visible or accessor)
    // For now, assume secondary_startup assembly sets up TTBR0/TCR/MAIR before calling here
    // or we do it here if MMU is off.
    
    // Initialize interrupt controller (GIC) for this CPU
    // gic_init_per_cpu();
    
    // Signal that we are up
    kinfo("CPU %u is up and running!\n", cpu_id);
    
    // Enter scheduler loop
    // sched_loop();
    while(1) { __asm__ volatile("wfi"); }
}

/**
 * Make a PSCI call
 */
static int64_t psci_call(uint64_t func_id, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
    register uint64_t x0 __asm__("x0") = func_id;
    register uint64_t x1 __asm__("x1") = arg0;
    register uint64_t x2 __asm__("x2") = arg1;
    register uint64_t x3 __asm__("x3") = arg2;
    
    // Try HVC (Hypervisor Call) - Common for QEMU/Virt
    __asm__ volatile("hvc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3));
    
    return (int64_t)x0;
}

/**
 * Power on a CPU
 */
static int cpu_on(uint64_t mpidr, uint64_t entry_point) {
    int64_t ret = psci_call(PSCI_0_2_FN_CPU_ON, mpidr, entry_point, 0);
    if (ret != PSCI_SUCCESS) {
        kerror("PSCI CPU_ON failed for MPIDR 0x%lx: %ld\n", mpidr, ret);
        return -1;
    }
    return 0;
}

/**
 * Initialize ARM64 CPU subsystem
 */
error_code_t cpu_init(void) {
    kinfo("Initializing ARM64 CPU subsystem...\n");
    
    // Initialize BSP
    init_bsp_cpu();
    
    // Parse device tree to find all CPUs
    extern dtb_node_t* dtb_get_root_node(void);
    extern dtb_node_t* dtb_find_node(const char* path);
    extern dtb_property_t* dtb_get_property(dtb_node_t* node, const char* name);
    
    dtb_node_t* root = dtb_get_root_node();
    if (!root) {
        kinfo("No device tree available, assuming 1 CPU\n");
        topology.cpu_count = 1;
        return ERR_OK;
    }
    
    dtb_node_t* cpus_node = dtb_find_node("/cpus");
    if (!cpus_node) {
        kinfo("No /cpus node found, assuming 1 CPU\n");
        topology.cpu_count = 1;
        return ERR_OK;
    }
    
    uint32_t count = 0;
    dtb_node_t* child = cpus_node->child;
    
    while (child && count < MAX_CPUS) {
        bool is_cpu = false;
        dtb_property_t* device_type = dtb_get_property(child, "device_type");
        if (device_type && strncmp((char*)device_type->data, "cpu", 3) == 0) {
            is_cpu = true;
        } else if (dtb_get_property(child, "reg")) {
            // Fallback: check if node name starts with "cpu"
            if (strncmp(child->name, "cpu", 3) == 0) {
                is_cpu = true;
            }
        }
        
        if (is_cpu) {
            dtb_property_t* reg = dtb_get_property(child, "reg");
            if (reg) {
                // reg contains MPIDR (usually 32-bit or 64-bit big-endian in DTB)
                // Assuming 32-bit for now or need byteswap
                // DTB values are big-endian!
                uint32_t* val_ptr = (uint32_t*)reg->data;
                uint32_t mpidr_be = *val_ptr;
                uint32_t mpidr = ((mpidr_be >> 24) & 0xFF) | 
                                 ((mpidr_be >> 8) & 0xFF00) | 
                                 ((mpidr_be << 8) & 0xFF0000) | 
                                 ((mpidr_be << 24) & 0xFF000000);
                                 
                // If 64-bit reg, might need to read more.
                // For QEMU virt, it's usually 64-bit address cells?
                // Let's assume standard 32-bit MPIDR in reg for now.
                
                // Skip BSP (already initialized)
                if (mpidr == topology.cpus[0].apic_id) { // apic_id stores MPIDR affinity
                    count++;
                    child = child->sibling;
                    continue;
                }
                
                kinfo("Found Secondary CPU: MPIDR=0x%x\n", mpidr);
                
                topology.cpus[count].cpu_id = count;
                topology.cpus[count].apic_id = mpidr;
                
                // Boot it!
                // We need the physical address of secondary_startup
                // Assuming secondary_startup is linked in kernel text
                // and we are running at virtual address.
                // We need to pass the PHYSICAL address to PSCI.
                extern uint64_t kernel_phys_offset; // Need this or similar
                // Or just assume identity map for now if early boot?
                // If we are in higher half, we need virt_to_phys.
                // Let's assume secondary_startup is a symbol.
                
                // For now, just log that we WOULD boot it.
                // To actually boot, we need secondary_startup assembly trampoline.
                // cpu_on(mpidr, (uint64_t)secondary_startup); 
                kinfo("Starting CPU %u (MPIDR 0x%x)...\n", count, mpidr);
                cpu_on(mpidr, (uint64_t)secondary_startup);
            }
            count++;
        }
        child = child->sibling;
    }
    
    topology.cpu_count = count;
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

