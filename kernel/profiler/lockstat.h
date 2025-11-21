/**
 * @file lockstat.h
 * @brief Lock Contention Statistics Tracker
 *
 * Tracks lock acquisitions, contentions, and wait times
 */

#ifndef KERNEL_PROFILER_LOCKSTAT_H
#define KERNEL_PROFILER_LOCKSTAT_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_TRACKED_LOCKS 256

// Lock statistics entry
typedef struct {
    const char* lock_name;
    void* lock_addr;
    uint64_t acquisitions;
    uint64_t contentions;
    uint64_t total_wait_time_ns;
    uint64_t max_wait_time_ns;
    uint64_t min_wait_time_ns;
    bool active;
} lock_stats_t;

// Lock statistics database
typedef struct {
    lock_stats_t locks[MAX_TRACKED_LOCKS];
    uint32_t count;
    bool enabled;
} lockstat_db_t;

// Initialize lock statistics
void lockstat_init(void);
void lockstat_cleanup(void);

// Enable/disable tracking
void lockstat_enable(void);
void lockstat_disable(void);

// Register a lock for tracking
int lockstat_register(void* lock, const char* name);
void lockstat_unregister(void* lock);

// Record lock events
void lockstat_record_acquisition(void* lock);
void lockstat_record_contention(void* lock, uint64_t wait_ns);

// Query statistics
const lock_stats_t* lockstat_get(void* lock);
void lockstat_get_all(lock_stats_t* buffer, uint32_t* count);

// Reset statistics
void lockstat_reset(void);
void lockstat_reset_lock(void* lock);

// Print statistics
void lockstat_print(void);
void lockstat_print_lock(void* lock);

#endif // KERNEL_PROFILER_LOCKSTAT_H
