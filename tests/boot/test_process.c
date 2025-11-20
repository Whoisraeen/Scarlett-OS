/**
 * @file test_process.c
 * @brief Test process creation and management
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/process.h"
#include "../../kernel/include/kprintf.h"
#include "../../kernel/include/debug.h"

/**
 * Test process creation
 */
void test_process_creation(void) {
    kinfo("=== Testing Process Creation ===\n");
    
    // Initialize process management
    process_init();
    kinfo("[PASS] Process management initialized\n");
    
    // Create a test process
    process_t* proc = process_create("test_process", 0x400000);
    if (!proc) {
        kerror("[FAIL] Process creation failed\n");
        return;
    }
    
    kinfo("[PASS] Process created: PID %d, name: %s\n", proc->pid, proc->name);
    kinfo("[PASS] Process entry point: 0x%016lx\n", proc->entry_point);
    kinfo("[PASS] Process address space: %p\n", proc->address_space);
    
    // Test process lookup
    process_t* found = process_get_by_pid(proc->pid);
    if (found != proc) {
        kerror("[FAIL] Process lookup failed\n");
        return;
    }
    
    kinfo("[PASS] Process lookup successful\n");
    
    // Test current process
    process_set_current(proc);
    process_t* current = process_get_current();
    if (current != proc) {
        kerror("[FAIL] Current process not set correctly\n");
        return;
    }
    
    kinfo("[PASS] Current process set correctly\n");
    
    // Cleanup
    process_destroy(proc);
    kinfo("[PASS] Process destroyed\n");
    
    kinfo("[PASS] Process creation test complete\n");
}

