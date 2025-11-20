/**
 * @file arch_select.h
 * @brief Architecture selection header
 * 
 * This header selects the appropriate HAL implementation based on architecture.
 */

#ifndef KERNEL_HAL_ARCH_SELECT_H
#define KERNEL_HAL_ARCH_SELECT_H

// Include architecture-specific HAL implementation
#if defined(__x86_64__) || defined(_M_X64)
    // x86_64 HAL implementation is in hal/x86_64/hal_impl.c
    // Functions are defined there
#elif defined(__aarch64__) || defined(_M_ARM64)
    // ARM64 HAL implementation is in hal/arm64/hal_impl.c
    // Functions are defined there
#elif defined(__riscv) && __riscv_xlen == 64
    // RISC-V HAL implementation (TODO)
    #error "RISC-V HAL not yet implemented"
#else
    #error "Unknown or unsupported architecture"
#endif

#endif // KERNEL_HAL_ARCH_SELECT_H

