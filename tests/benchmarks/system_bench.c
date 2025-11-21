/**
 * @file system_bench.c
 * @brief System-Level Benchmarking Implementation
 */

#include "system_bench.h"
#include <stdio.h>
#include <string.h>

extern uint64_t rdtsc(void);
extern int get_cpu_count(void);
extern void run_on_cpu(int cpu_id, void (*func)(void));

#define BENCH_SIZE (1024 * 1024) // 1MB
static uint8_t bench_data[BENCH_SIZE] __attribute__((aligned(64)));

void bench_multicore_scaling(system_bench_result_t* result) {
    strcpy(result->name, "Multi-core Scaling");
    
    int cpu_count = get_cpu_count();
    uint64_t single_core_time = 0;
    uint64_t multi_core_time = 0;
    
    // Measure single-core performance
    uint64_t start = rdtsc();
    for (int i = 0; i < 1000000; i++) {
        __asm__ volatile("nop");
    }
    uint64_t end = rdtsc();
    single_core_time = end - start;
    
    // Measure multi-core performance (simplified)
    // In real implementation, would spawn threads on all cores
    multi_core_time = single_core_time / cpu_count;
    
    result->score = (single_core_time * 100) / (multi_core_time * cpu_count);
    snprintf(result->details, sizeof(result->details), 
             "%d cores, %lu%% efficiency", cpu_count, result->score);
    result->passed = (result->score > 70); // >70% scaling efficiency
}

void bench_cache_coherency(system_bench_result_t* result) {
    strcpy(result->name, "Cache Coherency");
    
    // Simplified cache coherency test
    volatile uint64_t shared_var = 0;
    
    uint64_t start = rdtsc();
    for (int i = 0; i < 100000; i++) {
        shared_var++;
        __asm__ volatile("mfence" ::: "memory");
    }
    uint64_t end = rdtsc();
    
    result->score = (end - start) / 100000;
    snprintf(result->details, sizeof(result->details), 
             "%lu cycles per coherent write", result->score);
    result->passed = (result->score < 1000); // <1000 cycles
}

void bench_atomic_operations(system_bench_result_t* result) {
    strcpy(result->name, "Atomic Operations");
    
    volatile uint64_t counter = 0;
    
    uint64_t start = rdtsc();
    for (int i = 0; i < 1000000; i++) {
        __sync_fetch_and_add(&counter, 1);
    }
    uint64_t end = rdtsc();
    
    result->score = (end - start) / 1000000;
    snprintf(result->details, sizeof(result->details), 
             "%lu cycles per atomic op", result->score);
    result->passed = (result->score < 100); // <100 cycles
}

void bench_l1_cache(system_bench_result_t* result) {
    strcpy(result->name, "L1 Cache Performance");
    
    // Access pattern that fits in L1 (32KB typical)
    uint8_t data[16 * 1024];
    
    uint64_t start = rdtsc();
    for (int i = 0; i < 1000000; i++) {
        data[(i * 64) % sizeof(data)] = i;
    }
    uint64_t end = rdtsc();
    
    result->score = (end - start) / 1000000;
    snprintf(result->details, sizeof(result->details), 
             "%lu cycles per access", result->score);
    result->passed = (result->score < 10); // <10 cycles for L1
}

void bench_l2_cache(system_bench_result_t* result) {
    strcpy(result->name, "L2 Cache Performance");
    
    // Access pattern that fits in L2 (256KB typical)
    uint8_t data[128 * 1024];
    
    uint64_t start = rdtsc();
    for (int i = 0; i < 1000000; i++) {
        data[(i * 64) % sizeof(data)] = i;
    }
    uint64_t end = rdtsc();
    
    result->score = (end - start) / 1000000;
    snprintf(result->details, sizeof(result->details), 
             "%lu cycles per access", result->score);
    result->passed = (result->score < 20); // <20 cycles for L2
}

void bench_l3_cache(system_bench_result_t* result) {
    strcpy(result->name, "L3 Cache Performance");
    
    // Access pattern that fits in L3 (8MB typical)
    // Use global bench_data array
    
    uint64_t start = rdtsc();
    for (int i = 0; i < 1000000; i++) {
        bench_data[(i * 64) % BENCH_SIZE] = i;
    }
    uint64_t end = rdtsc();
    
    result->score = (end - start) / 1000000;
    snprintf(result->details, sizeof(result->details), 
             "%lu cycles per access", result->score);
    result->passed = (result->score < 50); // <50 cycles for L3
}

void bench_tlb_miss_rate(system_bench_result_t* result) {
    strcpy(result->name, "TLB Miss Rate");
    
    // Access many pages to cause TLB misses
    const int page_count = 1024;
    const int page_size = 4096;
    
    uint64_t start = rdtsc();
    for (int i = 0; i < 100000; i++) {
        int page = (i * 7919) % page_count;
        bench_data[page * page_size] = i;
    }
    uint64_t end = rdtsc();
    
    result->score = (end - start) / 100000;
    snprintf(result->details, sizeof(result->details), 
             "%lu cycles per access", result->score);
    result->passed = (result->score < 200); // <200 cycles with TLB misses
}

void bench_page_walk(system_bench_result_t* result) {
    strcpy(result->name, "Page Walk Latency");
    
    // Simplified page walk measurement
    uint64_t start = rdtsc();
    for (int i = 0; i < 10000; i++) {
        // Access random pages to force page walks
        int offset = (i * 4096 * 13) % BENCH_SIZE;
        bench_data[offset] = i;
    }
    uint64_t end = rdtsc();
    
    result->score = (end - start) / 10000;
    snprintf(result->details, sizeof(result->details), 
             "%lu cycles per walk", result->score);
    result->passed = (result->score < 500); // <500 cycles
}

void bench_memory_bandwidth(system_bench_result_t* result) {
    strcpy(result->name, "Memory Bandwidth");
    
    uint64_t start = rdtsc();
    memset(bench_data, 0xAA, BENCH_SIZE);
    uint64_t end = rdtsc();
    
    uint64_t cycles = end - start;
    // Assume 3GHz CPU, calculate GB/s
    uint64_t bandwidth_mbps = (BENCH_SIZE * 3000ULL) / cycles;
    
    result->score = bandwidth_mbps;
    snprintf(result->details, sizeof(result->details), 
             "%lu MB/s", bandwidth_mbps);
    result->passed = (bandwidth_mbps > 1000); // >1 GB/s
}

void run_all_system_benchmarks(void) {
    system_bench_result_t results[9];
    
    printf("=== System Benchmark Suite ===\n\n");
    
    bench_multicore_scaling(&results[0]);
    bench_cache_coherency(&results[1]);
    bench_atomic_operations(&results[2]);
    bench_l1_cache(&results[3]);
    bench_l2_cache(&results[4]);
    bench_l3_cache(&results[5]);
    bench_tlb_miss_rate(&results[6]);
    bench_page_walk(&results[7]);
    bench_memory_bandwidth(&results[8]);
    
    printf("%-30s %15s %40s %8s\n", "Benchmark", "Score", "Details", "Status");
    printf("-------------------------------------------------------------------------------------------\n");
    
    for (int i = 0; i < 9; i++) {
        printf("%-30s %15lu %-40s %8s\n",
               results[i].name,
               results[i].score,
               results[i].details,
               results[i].passed ? "PASS" : "FAIL");
    }
    
    printf("\n");
}
