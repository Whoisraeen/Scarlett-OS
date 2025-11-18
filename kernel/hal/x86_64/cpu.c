/**
 * @file cpu.c
 * @brief CPU detection and management for x86_64
 */

#include "../../include/types.h"
#include "../../include/cpu.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "../../include/mm/heap.h"

// CPU topology
static cpu_topology_t topology;
static per_cpu_data_t per_cpu_data[MAX_CPUS];

// Current CPU ID (set by each CPU during initialization)
// Use per-CPU data structure instead of __thread
static uint32_t current_cpu_id = 0;

/**
 * Execute CPUID instruction
 */
void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(subleaf));
}

/**
 * Read Local APIC ID
 */
static uint32_t read_lapic_id(void) {
    // Read from Local APIC ID register (offset 0x20)
    // For now, we'll use CPUID to get APIC ID
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    return (ebx >> 24) & 0xFF;  // APIC ID is in EBX[31:24]
}

/**
 * Check if CPU supports APIC
 */
static bool cpu_has_apic(void) {
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    return (edx & (1 << 9)) != 0;  // APIC bit in feature flags
}

/**
 * Get CPU vendor string
 */
static void get_cpu_vendor(char* vendor) {
    uint32_t eax, ebx, ecx, edx;
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    
    // Vendor string is in EBX, EDX, ECX
    uint32_t* v = (uint32_t*)vendor;
    v[0] = ebx;
    v[1] = edx;
    v[2] = ecx;
    vendor[12] = '\0';
}

/**
 * Detect number of logical processors
 */
static uint32_t detect_cpu_count(void) {
    uint32_t eax, ebx, ecx, edx;
    
    // Check if CPU supports CPUID leaf 0xB (Extended Topology Enumeration)
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    if (eax >= 0xB) {
        // Use extended topology enumeration
        cpuid(0xB, 0, &eax, &ebx, &ecx, &edx);
        if (ebx != 0) {
            return ebx & 0xFFFF;  // Number of logical processors at this level
        }
    }
    
    // Fallback: Use CPUID leaf 1 to get number of cores
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    
    // Check if CPU supports CPUID leaf 4 (Cache Parameters)
    if (eax >= 4) {
        cpuid(4, 0, &eax, &ebx, &ecx, &edx);
        uint32_t cores = ((eax >> 26) & 0x3F) + 1;  // Cores per package
        return cores;
    }
    
    // Default: assume 1 CPU if we can't detect
    return 1;
}

/**
 * Initialize CPU information for BSP
 */
static void init_bsp_cpu(void) {
    cpu_info_t* cpu = &topology.cpus[0];
    
    // Get CPU vendor
    get_cpu_vendor(cpu->vendor);
    
    // Get CPU family, model, stepping
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    
    cpu->family = ((eax >> 8) & 0xF) + ((eax >> 20) & 0xFF);
    cpu->model = ((eax >> 4) & 0xF) | ((eax >> 12) & 0xF0);
    cpu->stepping = eax & 0xF;
    cpu->features = edx;
    
    // Get APIC ID
    cpu->apic_id = read_lapic_id();
    
    // BSP detection (simplified - BSP is CPU 0)
    cpu->is_bsp = true;
    cpu->cpu_id = 0;
    cpu->state = CPU_STATE_BSP;
    
    // Initialize per-CPU data
    per_cpu_data[0].cpu_id = 0;
    per_cpu_data[0].info = cpu;
    per_cpu_data[0].kernel_stack = NULL;  // Will be set up later
    per_cpu_data[0].idle_stack = NULL;  // Will be set up later
    per_cpu_data[0].tsc_freq = 0;        // Will be calibrated later
    
    kinfo("CPU 0 (BSP): APIC ID %u, Vendor: %s, Family %u, Model %u\n",
          cpu->apic_id, cpu->vendor, cpu->family, cpu->model);
}

/**
 * Initialize CPU subsystem
 */
error_code_t cpu_init(void) {
    kinfo("Initializing CPU subsystem...\n");
    
    // Clear topology
    topology.num_cpus = 0;
    topology.num_cores = 0;
    topology.num_threads = 0;
    for (int i = 0; i < MAX_CPUS; i++) {
        topology.cpus[i].apic_id = 0;
        topology.cpus[i].state = CPU_STATE_UNKNOWN;
    }
    
    // Check if APIC is supported
    if (!cpu_has_apic()) {
        kwarn("CPU does not support APIC - SMP will not work\n");
        // Continue with single-core
    }
    
    // Detect CPU count
    uint32_t detected_count = detect_cpu_count();
    kinfo("Detected %u logical processor(s)\n", detected_count);
    
    // For now, we'll only initialize the BSP
    // APs will be initialized during AP startup
    topology.num_cpus = 1;  // Start with just BSP
    init_bsp_cpu();
    
    // Set current CPU ID
    current_cpu_id = 0;
    
    kinfo("CPU subsystem initialized (BSP only, APs will be started later)\n");
    return ERR_OK;
}

/**
 * Get CPU count
 */
uint32_t cpu_get_count(void) {
    return topology.num_cpus;
}

/**
 * Get CPU info by ID
 */
cpu_info_t* cpu_get_info(uint32_t cpu_id) {
    if (cpu_id >= MAX_CPUS) {
        return NULL;
    }
    return &topology.cpus[cpu_id];
}

/**
 * Get current CPU info
 */
cpu_info_t* cpu_get_current(void) {
    return &topology.cpus[current_cpu_id];
}

/**
 * Get current CPU ID
 */
uint32_t cpu_get_current_id(void) {
    return current_cpu_id;
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

/**
 * Get current CPU's per-CPU data
 */
per_cpu_data_t* cpu_get_current_per_cpu_data(void) {
    return &per_cpu_data[current_cpu_id];
}

/**
 * Get CPU topology
 */
cpu_topology_t* cpu_get_topology(void) {
    return &topology;
}

