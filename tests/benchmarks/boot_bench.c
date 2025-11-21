/**
 * @file boot_bench.c
 * @brief Boot Time Benchmarking Implementation
 */

#include "boot_bench.h"
#include <stdio.h>
#include <string.h>

static uint64_t boot_timestamps[BOOT_PHASE_MAX];
static bool boot_bench_initialized = false;

// Get current timestamp in microseconds
static uint64_t get_timestamp_us(void) {
    // TODO: Use actual timer
    // For now, use TSC and convert to microseconds
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    uint64_t tsc = ((uint64_t)high << 32) | low;
    
    // Assume 2GHz CPU for now (2000 cycles = 1 microsecond)
    return tsc / 2000;
}

void boot_bench_init(void) {
    memset(boot_timestamps, 0, sizeof(boot_timestamps));
    boot_timestamps[BOOT_PHASE_FIRMWARE] = 0;  // Baseline
    boot_bench_initialized = true;
}

void boot_bench_mark(boot_phase_t phase) {
    if (!boot_bench_initialized || phase >= BOOT_PHASE_MAX) {
        return;
    }
    
    boot_timestamps[phase] = get_timestamp_us();
}

void boot_bench_get_times(boot_times_t* times) {
    if (!boot_bench_initialized) {
        memset(times, 0, sizeof(boot_times_t));
        return;
    }
    
    times->firmware_to_kernel = boot_timestamps[BOOT_PHASE_KERNEL_ENTRY] - boot_timestamps[BOOT_PHASE_FIRMWARE];
    times->kernel_entry = boot_timestamps[BOOT_PHASE_KERNEL_ENTRY];
    times->mm_init = boot_timestamps[BOOT_PHASE_MM_INIT] - boot_timestamps[BOOT_PHASE_KERNEL_ENTRY];
    times->sched_init = boot_timestamps[BOOT_PHASE_SCHED_INIT] - boot_timestamps[BOOT_PHASE_MM_INIT];
    times->ipc_init = boot_timestamps[BOOT_PHASE_IPC_INIT] - boot_timestamps[BOOT_PHASE_SCHED_INIT];
    times->hal_init = boot_timestamps[BOOT_PHASE_HAL_INIT] - boot_timestamps[BOOT_PHASE_IPC_INIT];
    times->driver_init = boot_timestamps[BOOT_PHASE_DRIVER_INIT] - boot_timestamps[BOOT_PHASE_HAL_INIT];
    times->fs_init = boot_timestamps[BOOT_PHASE_FS_INIT] - boot_timestamps[BOOT_PHASE_DRIVER_INIT];
    times->network_init = boot_timestamps[BOOT_PHASE_NETWORK_INIT] - boot_timestamps[BOOT_PHASE_FS_INIT];
    times->gui_init = boot_timestamps[BOOT_PHASE_GUI_INIT] - boot_timestamps[BOOT_PHASE_NETWORK_INIT];
    times->service_startup = boot_timestamps[BOOT_PHASE_SERVICE_START] - boot_timestamps[BOOT_PHASE_GUI_INIT];
    times->desktop_load = boot_timestamps[BOOT_PHASE_DESKTOP_LOAD] - boot_timestamps[BOOT_PHASE_SERVICE_START];
    times->total = boot_timestamps[BOOT_PHASE_COMPLETE] - boot_timestamps[BOOT_PHASE_FIRMWARE];
}

void boot_bench_print(void) {
    boot_times_t times;
    boot_bench_get_times(&times);
    
    printf("\n=== Boot Time Benchmark ===\n");
    printf("Firmware to Kernel:  %6llu ms\n", times.firmware_to_kernel / 1000);
    printf("Memory Init:         %6llu ms\n", times.mm_init / 1000);
    printf("Scheduler Init:      %6llu ms\n", times.sched_init / 1000);
    printf("IPC Init:            %6llu ms\n", times.ipc_init / 1000);
    printf("HAL Init:            %6llu ms\n", times.hal_init / 1000);
    printf("Driver Init:         %6llu ms\n", times.driver_init / 1000);
    printf("File System Init:    %6llu ms\n", times.fs_init / 1000);
    printf("Network Init:        %6llu ms\n", times.network_init / 1000);
    printf("GUI Init:            %6llu ms\n", times.gui_init / 1000);
    printf("Service Startup:     %6llu ms\n", times.service_startup / 1000);
    printf("Desktop Load:        %6llu ms\n", times.desktop_load / 1000);
    printf("---------------------------\n");
    printf("Total Boot Time:     %6llu ms (%.2f seconds)\n", 
           times.total / 1000, (double)times.total / 1000000.0);
    
    if (boot_bench_target_met()) {
        printf("✓ Target met (< 10 seconds)\n");
    } else {
        printf("✗ Target not met (< 10 seconds)\n");
    }
    printf("===========================\n\n");
}

bool boot_bench_target_met(void) {
    boot_times_t times;
    boot_bench_get_times(&times);
    
    // Target: < 10 seconds (10,000,000 microseconds)
    return times.total < 10000000;
}
