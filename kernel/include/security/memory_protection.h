/**
 * @file memory_protection.h
 * @brief Memory protection mechanisms
 * 
 * Implements ASLR, stack canaries, and non-executable stack.
 */

#ifndef KERNEL_SECURITY_MEMORY_PROTECTION_H
#define KERNEL_SECURITY_MEMORY_PROTECTION_H

#include "../types.h"
#include "../errors.h"

// Stack canary value (randomized at boot)
extern uint64_t stack_canary;

/**
 * Initialize memory protection
 */
error_code_t memory_protection_init(void);

/**
 * Generate random value for ASLR
 */
uint64_t aslr_get_random_offset(void);

/**
 * Apply ASLR to address
 */
uint64_t aslr_apply(uint64_t base_address, uint64_t random_offset);

/**
 * Initialize stack canary
 */
void stack_canary_init(void);

/**
 * Get stack canary value
 */
uint64_t stack_canary_get(void);

/**
 * Verify stack canary
 */
bool stack_canary_verify(uint64_t canary);

/**
 * Set page as non-executable
 */
error_code_t set_page_nx(uint64_t address, bool nx);

/**
 * Enable/disable executable stack
 */
error_code_t set_stack_executable(bool executable);

/**
 * Protect kernel address space
 */
error_code_t protect_kernel_space(void);

#endif // KERNEL_SECURITY_MEMORY_PROTECTION_H

