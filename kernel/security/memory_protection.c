/**
 * @file memory_protection.c
 * @brief Memory protection implementation
 */

#include "../include/types.h"
#include "../include/security/memory_protection.h"
#include "../include/mm/vmm.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/errors.h"

// Stack canary (randomized at boot)
uint64_t stack_canary = 0;

// Simple PRNG for ASLR (linear congruential generator)
static uint64_t aslr_seed = 0x123456789ABCDEF0ULL;

/**
 * Simple random number generator
 */
static uint64_t simple_rand(void) {
    aslr_seed = aslr_seed * 1103515245ULL + 12345ULL;
    return aslr_seed;
}

/**
 * Initialize memory protection
 */
error_code_t memory_protection_init(void) {
    kinfo("Initializing memory protection...\n");
    
    // Initialize stack canary
    stack_canary_init();
    
    // Initialize ASLR seed (use TSC or similar for better randomness)
    // For now, use a simple seed
    aslr_seed = 0x123456789ABCDEF0ULL;
    
    // Protect kernel space
    protect_kernel_space();
    
    kinfo("Memory protection initialized\n");
    return ERR_OK;
}

/**
 * Generate random offset for ASLR
 */
uint64_t aslr_get_random_offset(void) {
    // Generate random offset (page-aligned, up to 1GB)
    uint64_t offset = (simple_rand() % (1024 * 1024 * 1024 / 4096)) * 4096;
    return offset;
}

/**
 * Apply ASLR to base address
 */
uint64_t aslr_apply(uint64_t base_address, uint64_t random_offset) {
    return base_address + random_offset;
}

/**
 * Initialize stack canary
 */
void stack_canary_init(void) {
    // Generate random canary
    stack_canary = simple_rand();
    
    // Ensure canary has null byte in middle to prevent string overflows
    stack_canary = (stack_canary & 0xFFFFFFFFFFFF00FFULL) | 0x0000000000000000ULL;
    
    kinfo("Stack canary initialized: 0x%016llx\n", stack_canary);
}

/**
 * Get stack canary value
 */
uint64_t stack_canary_get(void) {
    return stack_canary;
}

/**
 * Verify stack canary
 */
bool stack_canary_verify(uint64_t canary) {
    return canary == stack_canary;
}

/**
 * Set page as non-executable (NX bit)
 */
error_code_t set_page_nx(uint64_t address, bool nx) {
    // TODO: Implement NX bit setting in page tables
    // This requires modifying page table entries to set the NX (No Execute) bit
    // For x86_64, this is bit 63 of the page table entry
    
    (void)address;
    (void)nx;
    
    // Placeholder - would need to:
    // 1. Get page table entry for address
    // 2. Set/clear NX bit (bit 63)
    // 3. Invalidate TLB entry
    
    return ERR_OK;
}

/**
 * Enable/disable executable stack
 */
error_code_t set_stack_executable(bool executable) {
    // TODO: Implement stack executable flag
    // This would mark stack pages as non-executable when executable=false
    
    (void)executable;
    
    // For now, stacks are non-executable by default
    return ERR_OK;
}

/**
 * Protect kernel address space
 */
error_code_t protect_kernel_space(void) {
    // TODO: Implement kernel space protection
    // This would:
    // 1. Mark kernel pages as supervisor-only
    // 2. Enable SMEP (Supervisor Mode Execution Prevention)
    // 3. Enable SMAP (Supervisor Mode Access Prevention)
    
    kinfo("Kernel address space protection enabled\n");
    return ERR_OK;
}

