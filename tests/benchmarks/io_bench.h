/**
 * @file io_bench.h
 * @brief I/O Benchmarking Suite
 */

#ifndef IO_BENCH_H
#define IO_BENCH_H

#include <stdint.h>

typedef struct {
    char name[64];
    uint64_t bytes_per_second;
    uint64_t operations_per_second;
    uint32_t passed;
} io_bench_result_t;

// Disk I/O benchmarks
void bench_disk_sequential_read(io_bench_result_t* result);
void bench_disk_sequential_write(io_bench_result_t* result);
void bench_disk_random_read(io_bench_result_t* result);
void bench_disk_random_write(io_bench_result_t* result);

// Network I/O benchmarks
void bench_network_tcp_throughput(io_bench_result_t* result);
void bench_network_udp_throughput(io_bench_result_t* result);
void bench_network_latency(io_bench_result_t* result);

// File system benchmarks
void bench_fs_create_files(io_bench_result_t* result);
void bench_fs_delete_files(io_bench_result_t* result);
void bench_fs_stat_files(io_bench_result_t* result);

// Run all I/O benchmarks
void run_all_io_benchmarks(void);

#endif // IO_BENCH_H
