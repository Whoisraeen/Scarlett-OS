/**
 * @file micro_bench.h
 * @brief Microbenchmarks for Kernel Operations
 *
 * Measures latency and throughput of core kernel operations
 */

#ifndef TESTS_BENCHMARKS_MICRO_BENCH_H
#define TESTS_BENCHMARKS_MICRO_BENCH_H

#include <stdint.h>

// Benchmark results
typedef struct {
    const char* name;
    uint64_t iterations;
    uint64_t total_cycles;
    uint64_t min_cycles;
    uint64_t max_cycles;
    double avg_cycles;
    double avg_ns;  // Average nanoseconds
} bench_result_t;

// Initialize microbenchmarks
void micro_bench_init(void);

// Context switch benchmark
void micro_bench_context_switch(bench_result_t* result);

// System call overhead
void micro_bench_syscall(bench_result_t* result);

// IPC message passing
void micro_bench_ipc(bench_result_t* result);

// Memory allocation
void micro_bench_malloc(bench_result_t* result);

// Page fault handling
void micro_bench_page_fault(bench_result_t* result);

// Lock acquisition
void micro_bench_lock(bench_result_t* result);

// Interrupt latency
void micro_bench_interrupt(bench_result_t* result);

// Run all microbenchmarks
void micro_bench_run_all(void);

// Print benchmark results
void micro_bench_print(const bench_result_t* result);

#endif // TESTS_BENCHMARKS_MICRO_BENCH_H
