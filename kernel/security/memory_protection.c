/**
 * @file memory_protection.c
 * @brief Memory protection implementation
 */

#include "../include/types.h"
#include "../include/security/memory_protection.h"
#include "../include/mm/vmm.h"
#include "../include/config.h"
#include "../include/process.h"
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
    // Get current process's address space
    process_t* current_process = process_get_current();
    address_space_t* as = NULL;
    
    if (current_process && current_process->address_space) {
        as = current_process->address_space;
    } else {
        // Fall back to kernel address space
        as = vmm_get_kernel_address_space();
    }
    
    if (!as) {
        return ERR_INVALID_STATE;
    }
    
    // Get page table entry for this address
    // We need to access the page table directly
    uint64_t* pml4 = as->pml4;
    
    // Extract page table indices
    uint64_t pml4_idx = (address >> 39) & 0x1FF;
    uint64_t pdp_idx = (address >> 30) & 0x1FF;
    uint64_t pd_idx = (address >> 21) & 0x1FF;
    uint64_t pt_idx = (address >> 12) & 0x1FF;
    
    // Traverse page tables
    if (!(pml4[pml4_idx] & VMM_PRESENT)) {
        return ERR_NOT_FOUND;
    }
    
    uint64_t* pdp = (uint64_t*)(pml4[pml4_idx] & 0xFFFFFFFFF000ULL);
    if (!(pdp[pdp_idx] & VMM_PRESENT)) {
        return ERR_NOT_FOUND;
    }
    
    // Check if this is a 2MB page
    if (pdp[pdp_idx] & VMM_HUGE) {
        // 2MB page - modify PDP entry
        if (nx) {
            pdp[pdp_idx] |= VMM_NX;
        } else {
            pdp[pdp_idx] &= ~VMM_NX;
        }
        vmm_flush_tlb_single(address);
        return ERR_OK;
    }
    
    uint64_t* pd = (uint64_t*)(pdp[pdp_idx] & 0xFFFFFFFFF000ULL);
    if (!(pd[pd_idx] & VMM_PRESENT)) {
        return ERR_NOT_FOUND;
    }
    
    // Check if this is a 1GB page
    if (pd[pd_idx] & VMM_HUGE) {
        // 1GB page - modify PD entry
        if (nx) {
            pd[pd_idx] |= VMM_NX;
        } else {
            pd[pd_idx] &= ~VMM_NX;
        }
        vmm_flush_tlb_single(address);
        return ERR_OK;
    }
    
    // 4KB page - modify PT entry
    uint64_t* pt = (uint64_t*)(pd[pd_idx] & 0xFFFFFFFFF000ULL);
    if (!(pt[pt_idx] & VMM_PRESENT)) {
        return ERR_NOT_FOUND;
    }
    
    if (nx) {
        pt[pt_idx] |= VMM_NX;
    } else {
        pt[pt_idx] &= ~VMM_NX;
    }
    
    vmm_flush_tlb_single(address);
    return ERR_OK;
}

/**
 * Enable/disable executable stack
 */
error_code_t set_stack_executable(bool executable) {
    // Get current process
    process_t* current_process = process_get_current();
    if (!current_process || !current_process->address_space) {
        return ERR_INVALID_STATE;
    }
    
    // Mark all stack pages as executable or non-executable
    size_t stack_pages = (current_process->stack_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < stack_pages; i++) {
        vaddr_t stack_vaddr = current_process->stack_base + (i * PAGE_SIZE);
        error_code_t err = set_page_nx(stack_vaddr, !executable);
        if (err != ERR_OK) {
            kerror("Memory protection: Failed to set stack page NX bit\n");
            return err;
        }
    }
    
    return ERR_OK;
}

/**
 * Protect kernel address space
 */
error_code_t protect_kernel_space(void) {
    // Enable SMEP (Supervisor Mode Execution Prevention)
    // SMEP prevents kernel from executing code in user pages
    // Bit 20 in CR4 register
    
    // Enable SMAP (Supervisor Mode Access Prevention)
    // SMAP prevents kernel from accessing user pages
    // Bit 21 in CR4 register
    
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    
    // Check CPUID for SMEP support (CPUID.7.0.EBX bit 7)
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(7), "c"(0));
    
    if (ebx & (1 << 7)) {
        // SMEP supported
        cr4 |= (1ULL << 20);
        kinfo("Memory protection: SMEP enabled\n");
    } else {
        kinfo("Memory protection: SMEP not supported by CPU\n");
    }
    
    if (ebx & (1 << 20)) {
        // SMAP supported
        cr4 |= (1ULL << 21);
        kinfo("Memory protection: SMAP enabled\n");
    } else {
        kinfo("Memory protection: SMAP not supported by CPU\n");
    }
    
    // Write CR4 back
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));
    
    kinfo("Kernel address space protection enabled\n");
    return ERR_OK;
}

