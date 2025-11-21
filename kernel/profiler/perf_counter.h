/**
 * @file perf_counter.h
 * @brief Performance Counter Integration for Kernel Profiling
 *
 * Provides access to CPU performance monitoring counters
 */

#ifndef KERNEL_PROFILER_PERF_COUNTER_H
#define KERNEL_PROFILER_PERF_COUNTER_H

#include <stdint.h>
#include <stdbool.h>

// Performance counter types
typedef enum {
    PERF_CYCLES = 0,
    PERF_INSTRUCTIONS,
    PERF_CACHE_REFERENCES,
    PERF_CACHE_MISSES,
    PERF_BRANCH_INSTRUCTIONS,
    PERF_BRANCH_MISSES,
    PERF_TLB_REFERENCES,
    PERF_TLB_MISSES,
    PERF_COUNTER_MAX
} perf_counter_type_t;

// Performance counter snapshot
typedef struct {
    uint64_t cycles;
    uint64_t instructions;
    uint64_t cache_references;
    uint64_t cache_misses;
    uint64_t branch_instructions;
    uint64_t branch_misses;
    uint64_t tlb_references;
    uint64_t tlb_misses;
    uint64_t timestamp;
} perf_counters_t;

// Performance statistics
typedef struct {
    uint64_t delta_cycles;
    uint64_t delta_instructions;
    double ipc;  // Instructions per cycle
    double cache_miss_rate;
    double branch_miss_rate;
    double tlb_miss_rate;
} perf_stats_t;

// Initialize performance counters
int perf_counter_init(void);
void perf_counter_cleanup(void);

// Per-CPU initialization
int perf_counter_init_cpu(uint32_t cpu);

// Counter operations
void perf_counter_start(perf_counters_t* counters);
void perf_counter_stop(perf_counters_t* counters);
void perf_counter_read(perf_counters_t* counters);

// Calculate statistics
void perf_calculate_stats(const perf_counters_t* start, const perf_counters_t* end, perf_stats_t* stats);

// Enable/disable specific counters
int perf_counter_enable(perf_counter_type_t type);
int perf_counter_disable(perf_counter_type_t type);

// Read individual counter
uint64_t perf_counter_read_single(perf_counter_type_t type);

// Reset counters
void perf_counter_reset(void);

// Print statistics
void perf_print_stats(const perf_stats_t* stats);

#endif // KERNEL_PROFILER_PERF_COUNTER_H
