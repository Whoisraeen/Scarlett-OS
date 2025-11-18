/**
 * @file cpu.h
 * @brief CPU detection and management
 */

#ifndef KERNEL_CPU_H
#define KERNEL_CPU_H

#include "../types.h"

// Maximum number of CPUs
#define MAX_CPUS 256

// CPU states
typedef enum {
    CPU_STATE_UNKNOWN,
    CPU_STATE_BSP,      // Bootstrap Processor
    CPU_STATE_AP,       // Application Processor
    CPU_STATE_ONLINE,
    CPU_STATE_OFFLINE,
    CPU_STATE_HALTED
} cpu_state_t;

// CPU information structure
typedef struct {
    uint32_t apic_id;           // Local APIC ID
    uint32_t lapic_base;        // Local APIC base address
    cpu_state_t state;           // Current state
    uint32_t cpu_id;            // Logical CPU ID (0, 1, 2, ...)
    bool is_bsp;                // Is this the bootstrap processor?
    char vendor[13];             // CPU vendor string
    uint32_t family;             // CPU family
    uint32_t model;              // CPU model
    uint32_t stepping;          // CPU stepping
    uint32_t features;           // CPU features (CPUID feature flags)
} cpu_info_t;

// CPU topology
typedef struct {
    uint32_t num_cpus;           // Total number of CPUs detected
    uint32_t num_cores;          // Number of physical cores
    uint32_t num_threads;        // Number of threads per core
    cpu_info_t cpus[MAX_CPUS];   // CPU information array
} cpu_topology_t;

// Per-CPU data structure
typedef struct {
    uint32_t cpu_id;             // CPU ID
    cpu_info_t* info;            // Pointer to CPU info
    void* kernel_stack;          // Per-CPU kernel stack
    void* idle_stack;            // Per-CPU idle thread stack
    uint64_t tsc_freq;           // TSC frequency
} per_cpu_data_t;

// CPU functions
error_code_t cpu_init(void);
uint32_t cpu_get_count(void);
cpu_info_t* cpu_get_info(uint32_t cpu_id);
cpu_info_t* cpu_get_current(void);
uint32_t cpu_get_current_id(void);
per_cpu_data_t* cpu_get_per_cpu_data(uint32_t cpu_id);
per_cpu_data_t* cpu_get_current_per_cpu_data(void);

// CPUID wrapper
void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx);

// CPU topology
cpu_topology_t* cpu_get_topology(void);

#endif // KERNEL_CPU_H

