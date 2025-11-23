/**
 * @file context.h
 * @brief Architecture-specific execution context
 */

#ifndef KERNEL_HAL_CONTEXT_H
#define KERNEL_HAL_CONTEXT_H

#include "../types.h"

// Generic context handle (void* internally, but typed for safety)
typedef void* context_t;

#if defined(ARCH_X86_64)
typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rip;
    uint64_t rflags;
} arch_context_t;
#elif defined(ARCH_ARM64)
typedef struct {
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t x29; // FP
    uint64_t x30; // LR
    uint64_t sp;
    uint64_t pc;
} arch_context_t;
#else
// Stub for other architectures
typedef struct {
    uint64_t pc;
    uint64_t sp;
} arch_context_t;
#endif

#endif // KERNEL_HAL_CONTEXT_H
