/**
 * @file hal_common.c
 * @brief Common HAL utilities and architecture detection
 */

#include "../include/types.h"
#include "../include/hal/hal.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

// Architecture-specific HAL implementations
#if defined(__x86_64__) || defined(_M_X64)
    #include "x86_64/hal_impl.c"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #include "arm64/hal_impl.c"
#elif defined(__riscv) && __riscv_xlen == 64
    // TODO: RISC-V HAL implementation
    #error "RISC-V HAL not yet implemented"
#else
    #error "Unknown architecture"
#endif

// Current architecture (detected at boot)
static architecture_t current_arch = ARCH_UNKNOWN;

/**
 * Detect current architecture
 */
architecture_t hal_detect_architecture(void) {
    if (current_arch != ARCH_UNKNOWN) {
        return current_arch;
    }
    
    // Detect architecture at compile time or runtime
    #if defined(__x86_64__) || defined(_M_X64)
        current_arch = ARCH_X86_64;
    #elif defined(__aarch64__) || defined(_M_ARM64)
        current_arch = ARCH_ARM64;
    #elif defined(__riscv) && __riscv_xlen == 64
        current_arch = ARCH_RISCV;
    #else
        current_arch = ARCH_UNKNOWN;
    #endif
    
    return current_arch;
}

/**
 * Get architecture name string
 */
const char* hal_get_architecture_name(architecture_t arch) {
    switch (arch) {
        case ARCH_X86_64:
            return "x86_64";
        case ARCH_ARM64:
            return "ARM64";
        case ARCH_RISCV:
            return "RISC-V";
        default:
            return "Unknown";
    }
}

