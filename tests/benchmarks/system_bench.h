/**
 * @file system_bench.h
 * @brief System-Level Benchmarking Suite
 */

#ifndef SYSTEM_BENCH_H
#define SYSTEM_BENCH_H

#include <stdint.h>

typedef struct {
    char name[64];
    uint64_t score;
    uint32_t passed;
    char details[128];
} system_bench_result_t;

// Multi-core benchmarks
void bench_multicore_scaling(system_bench_result_t* result);
void bench_cache_coherency(system_bench_result_t* result);
void bench_atomic_operations(system_bench_result_t* result);

// Cache benchmarks
void bench_l1_cache(system_bench_result_t* result);
void bench_l2_cache(system_bench_result_t* result);
void bench_l3_cache(system_bench_result_t* result);

// TLB benchmarks
void bench_tlb_miss_rate(system_bench_result_t* result);
void bench_page_walk(system_bench_result_t* result);

// Memory bandwidth
void bench_memory_bandwidth(system_bench_result_t* result);

// Run all system benchmarks
void run_all_system_benchmarks(void);

#endif // SYSTEM_BENCH_H
