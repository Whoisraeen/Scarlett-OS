/**
 * @file arch_detect.c
 * @brief Architecture detection and initialization
 */

#include "../include/types.h"
#include "../include/hal/hal.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

// Architecture-specific HAL function pointers
// These will be set to point to the correct implementation
static struct {
    error_code_t (*cpu_init)(void);
    error_code_t (*interrupts_init)(void);
    error_code_t (*timer_init)(void);
    error_code_t (*syscall_init)(void);
    error_code_t (*serial_init)(void);
    error_code_t (*mm_init)(void);
    error_code_t (*early_init)(void);
    error_code_t (*late_init)(void);
} hal_ops;

/**
 * Initialize HAL based on detected architecture
 */
error_code_t hal_init(void) {
    architecture_t arch = hal_detect_architecture();
    
    kinfo("Detected architecture: %s\n", hal_get_architecture_name(arch));
    
    if (arch == ARCH_UNKNOWN) {
        kerror("Unknown architecture - cannot initialize HAL\n");
        return ERR_NOT_SUPPORTED;
    }
    
    // Architecture-specific initialization happens via direct calls
    // The HAL interface functions are implemented in arch-specific files
    
    return ERR_OK;
}

