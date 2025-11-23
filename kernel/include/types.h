/**
 * @file types.h
 * @brief Basic type definitions for the kernel
 */

#ifndef KERNEL_TYPES_H
#define KERNEL_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Physical and virtual addresses
typedef uint64_t paddr_t;
typedef uint64_t vaddr_t;

// Page frame number
typedef uint64_t pfn_t;

// Process ID
typedef int32_t pid_t;

// Signed size type
typedef int64_t ssize_t;

// NULL definition
#ifndef NULL
#define NULL ((void*)0)
#endif

// Useful macros
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))
#define IS_ALIGNED(x, align) (((x) & ((align) - 1)) == 0)

// Min/Max macros
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Bit manipulation
#define BIT(n) (1ULL << (n))
#define BITMASK(bits) ((1ULL << (bits)) - 1)

#endif // KERNEL_TYPES_H

