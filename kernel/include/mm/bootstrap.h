/**
 * @file bootstrap.h
 * @brief Bootstrap allocator for early boot
 *
 * Simple bump allocator used before the heap is initialized.
 * Used by VMM to allocate initial page tables.
 */

#ifndef KERNEL_MM_BOOTSTRAP_H
#define KERNEL_MM_BOOTSTRAP_H

#include "../types.h"
#include <stdbool.h>

/**
 * Allocate memory from bootstrap heap
 *
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void* bootstrap_alloc(size_t size);

/**
 * Zero-initialized bootstrap allocation
 *
 * @param size Number of bytes to allocate
 * @return Pointer to zeroed memory, or NULL on failure
 */
void* bootstrap_zalloc(size_t size);

/**
 * Disable bootstrap allocator
 *
 * Called after heap is initialized. After this, bootstrap_alloc will panic.
 */
void bootstrap_disable(void);

/**
 * Check if bootstrap allocator is active
 *
 * @return true if bootstrap allocator can still be used
 */
bool bootstrap_is_active(void);

/**
 * Get bootstrap allocator statistics
 *
 * @param used_out Output: bytes used
 * @param free_out Output: bytes remaining
 */
void bootstrap_get_stats(size_t* used_out, size_t* free_out);

#endif // KERNEL_MM_BOOTSTRAP_H
