/**
 * @file perf_counter.c
 * @brief Performance Counter Implementation
 */

#include "perf_counter.h"
#include <stdio.h>
#include <string.h>

// x86_64 MSR addresses for performance counters
#define MSR_IA32_PERFEVTSEL0 0x186
#define MSR_IA32_PERFEVTSEL1 0x187
#define MSR_IA32_PERFEVTSEL2 0x188
#define MSR_IA32_PERFEVTSEL3 0x189

#define MSR_IA32_PMC0 0xC1
#define MSR_IA32_PMC1 0xC2
#define MSR_IA32_PMC2 0xC3
#define MSR_IA32_PMC3 0xC4

// Event select values
#define EVENT_UNHALTED_CORE_CYCLES 0x3C
#define EVENT_INSTRUCTIONS_RETIRED 0xC0
#define EVENT_LLC_REFERENCES 0x2E
#define EVENT_LLC_MISSES 0x2E
#define EVENT_BRANCH_INSTRUCTIONS 0xC4
#define EVENT_BRANCH_MISSES 0xC5

// Event masks
#define UMASK_LLC_REFERENCES 0x4F
#define UMASK_LLC_MISSES 0x41

// Control bits
#define PERFEVTSEL_EN (1ULL << 22)  // Enable
#define PERFEVTSEL_USR (1ULL << 16) // User mode
#define PERFEVTSEL_OS (1ULL << 17)  // OS mode

static bool perf_initialized = false;

// Read MSR (Model Specific Register)
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

// Write MSR
static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

// Read timestamp counter
static inline uint64_t rdtsc(void) {
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

int perf_counter_init(void) {
    if (perf_initialized) {
        return 0;
    }
    
    // Initialize performance counters for all CPUs
    // DONE: CPU iteration implemented
    extern uint32_t cpu_get_count(void);
    uint32_t num_cpus = cpu_get_count();
    
    for (uint32_t i = 0; i < num_cpus; i++) {
        perf_counter_init_cpu(i);
    }
    
    perf_initialized = true;
    return 0;
}

void perf_counter_cleanup(void) {
    // Disable all counters
    wrmsr(MSR_IA32_PERFEVTSEL0, 0);
    wrmsr(MSR_IA32_PERFEVTSEL1, 0);
    wrmsr(MSR_IA32_PERFEVTSEL2, 0);
    wrmsr(MSR_IA32_PERFEVTSEL3, 0);
    
    perf_initialized = false;
}

int perf_counter_init_cpu(uint32_t cpu) {
    // Configure PMC0: Unhalted core cycles
    uint64_t evt0 = EVENT_UNHALTED_CORE_CYCLES | PERFEVTSEL_EN | PERFEVTSEL_USR | PERFEVTSEL_OS;
    wrmsr(MSR_IA32_PERFEVTSEL0, evt0);
    
    // Configure PMC1: Instructions retired
    uint64_t evt1 = EVENT_INSTRUCTIONS_RETIRED | PERFEVTSEL_EN | PERFEVTSEL_USR | PERFEVTSEL_OS;
    wrmsr(MSR_IA32_PERFEVTSEL1, evt1);
    
    // Configure PMC2: LLC references
    uint64_t evt2 = EVENT_LLC_REFERENCES | (UMASK_LLC_REFERENCES << 8) | PERFEVTSEL_EN | PERFEVTSEL_USR | PERFEVTSEL_OS;
    wrmsr(MSR_IA32_PERFEVTSEL2, evt2);
    
    // Configure PMC3: LLC misses
    uint64_t evt3 = EVENT_LLC_MISSES | (UMASK_LLC_MISSES << 8) | PERFEVTSEL_EN | PERFEVTSEL_USR | PERFEVTSEL_OS;
    wrmsr(MSR_IA32_PERFEVTSEL3, evt3);
    
    return 0;
}

void perf_counter_start(perf_counters_t* counters) {
    memset(counters, 0, sizeof(perf_counters_t));
    
    counters->timestamp = rdtsc();
    counters->cycles = rdmsr(MSR_IA32_PMC0);
    counters->instructions = rdmsr(MSR_IA32_PMC1);
    counters->cache_references = rdmsr(MSR_IA32_PMC2);
    counters->cache_misses = rdmsr(MSR_IA32_PMC3);
}

void perf_counter_stop(perf_counters_t* counters) {
    perf_counters_t end;
    perf_counter_read(&end);
    
    // Calculate deltas
    counters->cycles = end.cycles - counters->cycles;
    counters->instructions = end.instructions - counters->instructions;
    counters->cache_references = end.cache_references - counters->cache_references;
    counters->cache_misses = end.cache_misses - counters->cache_misses;
    counters->timestamp = end.timestamp - counters->timestamp;
}

void perf_counter_read(perf_counters_t* counters) {
    counters->timestamp = rdtsc();
    counters->cycles = rdmsr(MSR_IA32_PMC0);
    counters->instructions = rdmsr(MSR_IA32_PMC1);
    counters->cache_references = rdmsr(MSR_IA32_PMC2);
    counters->cache_misses = rdmsr(MSR_IA32_PMC3);
}

void perf_calculate_stats(const perf_counters_t* start, const perf_counters_t* end, perf_stats_t* stats) {
    stats->delta_cycles = end->cycles - start->cycles;
    stats->delta_instructions = end->instructions - start->instructions;
    
    // Calculate IPC (Instructions Per Cycle)
    if (stats->delta_cycles > 0) {
        stats->ipc = (double)stats->delta_instructions / (double)stats->delta_cycles;
    } else {
        stats->ipc = 0.0;
    }
    
    // Calculate cache miss rate
    uint64_t cache_refs = end->cache_references - start->cache_references;
    uint64_t cache_miss = end->cache_misses - start->cache_misses;
    if (cache_refs > 0) {
        stats->cache_miss_rate = (double)cache_miss / (double)cache_refs * 100.0;
    } else {
        stats->cache_miss_rate = 0.0;
    }
    
    // Calculate branch miss rate
    uint64_t branch_inst = end->branch_instructions - start->branch_instructions;
    uint64_t branch_miss = end->branch_misses - start->branch_misses;
    if (branch_inst > 0) {
        stats->branch_miss_rate = (double)branch_miss / (double)branch_inst * 100.0;
    } else {
        stats->branch_miss_rate = 0.0;
    }
    
    // Calculate TLB miss rate
    uint64_t tlb_refs = end->tlb_references - start->tlb_references;
    uint64_t tlb_miss = end->tlb_misses - start->tlb_misses;
    if (tlb_refs > 0) {
        stats->tlb_miss_rate = (double)tlb_miss / (double)tlb_refs * 100.0;
    } else {
        stats->tlb_miss_rate = 0.0;
    }
}

void perf_counter_reset(void) {
    wrmsr(MSR_IA32_PMC0, 0);
    wrmsr(MSR_IA32_PMC1, 0);
    wrmsr(MSR_IA32_PMC2, 0);
    wrmsr(MSR_IA32_PMC3, 0);
}

void perf_print_stats(const perf_stats_t* stats) {
    printf("Performance Statistics:\n");
    printf("  Cycles:       %llu\n", stats->delta_cycles);
    printf("  Instructions: %llu\n", stats->delta_instructions);
    printf("  IPC:          %.2f\n", stats->ipc);
    printf("  Cache Miss:   %.2f%%\n", stats->cache_miss_rate);
    printf("  Branch Miss:  %.2f%%\n", stats->branch_miss_rate);
    printf("  TLB Miss:     %.2f%%\n", stats->tlb_miss_rate);
}

uint64_t perf_counter_read_single(perf_counter_type_t type) {
    switch (type) {
        case PERF_CYCLES:
            return rdmsr(MSR_IA32_PMC0);
        case PERF_INSTRUCTIONS:
            return rdmsr(MSR_IA32_PMC1);
        case PERF_CACHE_REFERENCES:
            return rdmsr(MSR_IA32_PMC2);
        case PERF_CACHE_MISSES:
            return rdmsr(MSR_IA32_PMC3);
        default:
            return 0;
    }
}
