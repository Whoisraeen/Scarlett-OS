/**
 * @file hal_wrapper.c
 * @brief HAL wrapper that selects architecture-specific implementation
 */

#include "../include/types.h"
#include "../include/hal/hal.h"

// This file provides a single point to include the correct HAL implementation
// The actual implementation is in hal_common.c which includes the arch-specific file

// Architecture detection happens at compile time via preprocessor
// Runtime detection can be added later if needed

