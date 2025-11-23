/**
 * @file hal_common.c
 * @brief Common HAL utilities and architecture detection
 */

#include "../include/types.h"
#include "../include/hal/hal.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

// Architecture-specific HAL implementations
// Note: Implementation files are compiled separately by the build system
// Do not include them here to avoid redefinition errors

/**
 * Get architecture name string
 */
const char* hal_get_architecture_name(architecture_t arch) {
    switch (arch) {
        case HAL_ARCH_X86_64:
            return "x86_64";
        case HAL_ARCH_ARM64:
            return "ARM64";
        case HAL_ARCH_RISCV:
            return "RISC-V";
        default:
            return "Unknown";
    }
}
