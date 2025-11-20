/**
 * @file gdt.c
 * @brief ARM64 doesn't use GDT (x86-specific)
 * This file exists for compatibility with HAL interface
 */

#include "../../include/types.h"
#include "../../include/errors.h"

/**
 * ARM64 doesn't use GDT - this is a no-op
 */
error_code_t gdt_init(void) {
    // ARM64 uses different memory management (page tables only)
    // No segment descriptors needed
    return ERR_OK;
}

