/**
 * @file slab.h
 * @brief Slab allocator for kernel objects
 */

#ifndef KERNEL_MM_SLAB_H
#define KERNEL_MM_SLAB_H

#include "../types.h"

// Common object sizes (power of 2 for efficiency)
#define SLAB_SIZE_8     8
#define SLAB_SIZE_16    16
#define SLAB_SIZE_32    32
#define SLAB_SIZE_64    64
#define SLAB_SIZE_128   128
#define SLAB_SIZE_256   256
#define SLAB_SIZE_512   512
#define SLAB_SIZE_1024  1024
#define SLAB_SIZE_2048  2048
#define SLAB_SIZE_4096  4096

#define NUM_SLAB_SIZES 11

/**
 * Initialize slab allocator
 */
void slab_init(void);

/**
 * Allocate from slab (for objects <= 4KB)
 * @param size Size in bytes (will be rounded up to nearest slab size)
 * @return Pointer to allocated memory, or NULL on error
 */
void* slab_alloc(size_t size);

/**
 * Free memory allocated by slab_alloc
 * @param ptr Pointer to memory
 * @param size Original size (for validation)
 */
void slab_free(void* ptr, size_t size);

/**
 * Try to free a pointer (auto-detect size class)
 * @param ptr Pointer to memory
 * @return true if successfully freed, false if not a slab allocation
 */
bool slab_try_free(void* ptr);

/**
 * Get slab statistics
 */
void slab_get_stats(size_t* total_objects, size_t* free_objects, size_t* used_objects);

#endif // KERNEL_MM_SLAB_H

