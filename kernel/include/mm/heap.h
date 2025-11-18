/**
 * @file heap.h
 * @brief Kernel heap allocator interface
 */

#ifndef KERNEL_MM_HEAP_H
#define KERNEL_MM_HEAP_H

#include "../types.h"

/**
 * Initialize kernel heap
 */
void heap_init(void);

/**
 * Allocate memory from heap
 * @param size Size in bytes
 * @return Pointer to allocated memory, or NULL if out of memory
 */
void* kmalloc(size_t size);

/**
 * Allocate zeroed memory from heap
 * @param size Size in bytes
 * @return Pointer to allocated memory, or NULL if out of memory
 */
void* kzalloc(size_t size);

/**
 * Free memory allocated by kmalloc
 * @param ptr Pointer to memory to free
 */
void kfree(void* ptr);

/**
 * Reallocate memory
 * @param ptr Existing pointer (or NULL)
 * @param new_size New size
 * @return Pointer to reallocated memory
 */
void* krealloc(void* ptr, size_t new_size);

/**
 * Get heap statistics
 */
void heap_get_stats(size_t* total_size, size_t* used_size, size_t* free_size);

#endif // KERNEL_MM_HEAP_H

