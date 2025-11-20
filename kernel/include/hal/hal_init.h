/**
 * @file hal_init.h
 * @brief HAL initialization interface
 */

#ifndef KERNEL_HAL_HAL_INIT_H
#define KERNEL_HAL_HAL_INIT_H

#include "../types.h"
#include "../errors.h"

/**
 * Initialize HAL subsystem
 */
error_code_t hal_init(void);

#endif // KERNEL_HAL_HAL_INIT_H

