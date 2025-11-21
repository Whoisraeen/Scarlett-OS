/**
 * @file boot_bench.h
 * @brief Boot Time Benchmarking
 *
 * Measures boot time from firmware to desktop
 */

#ifndef TESTS_BENCHMARKS_BOOT_BENCH_H
#define TESTS_BENCHMARKS_BOOT_BENCH_H

#include <stdint.h>

// Boot phase identifiers
typedef enum {
    BOOT_PHASE_FIRMWARE = 0,
    BOOT_PHASE_KERNEL_ENTRY,
    BOOT_PHASE_MM_INIT,
    BOOT_PHASE_SCHED_INIT,
    BOOT_PHASE_IPC_INIT,
    BOOT_PHASE_HAL_INIT,
    BOOT_PHASE_DRIVER_INIT,
    BOOT_PHASE_FS_INIT,
    BOOT_PHASE_NETWORK_INIT,
    BOOT_PHASE_GUI_INIT,
    BOOT_PHASE_SERVICE_START,
    BOOT_PHASE_DESKTOP_LOAD,
    BOOT_PHASE_COMPLETE,
    BOOT_PHASE_MAX
} boot_phase_t;

// Boot time measurements
typedef struct {
    uint64_t firmware_to_kernel;
    uint64_t kernel_entry;
    uint64_t mm_init;
    uint64_t sched_init;
    uint64_t ipc_init;
    uint64_t hal_init;
    uint64_t driver_init;
    uint64_t fs_init;
    uint64_t network_init;
    uint64_t gui_init;
    uint64_t service_startup;
    uint64_t desktop_load;
    uint64_t total;
} boot_times_t;

// Initialize boot benchmarking
void boot_bench_init(void);

// Mark boot phase completion
void boot_bench_mark(boot_phase_t phase);

// Get boot times
void boot_bench_get_times(boot_times_t* times);

// Print boot times
void boot_bench_print(void);

// Check if boot time target met (< 10 seconds)
bool boot_bench_target_met(void);

#endif // TESTS_BENCHMARKS_BOOT_BENCH_H
