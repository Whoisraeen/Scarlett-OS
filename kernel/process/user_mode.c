/**
 * @file user_mode.c
 * @brief User mode transition support
 * 
 * Functions to prepare and execute user mode programs.
 */

#include "../include/types.h"
#include "../include/process.h"
#include "../include/mm/vmm.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

// External assembly function
extern void enter_user_mode(vaddr_t entry_point, vaddr_t user_stack, uint64_t rflags);

/**
 * Start a process in user mode
 * 
 * This function prepares the process for execution and switches to user mode.
 */
void process_start_user_mode(process_t* process) {
    if (!process) {
        kerror("User mode: Invalid process\n");
        return;
    }
    
    if (!process->address_space) {
        kerror("User mode: Process has no address space\n");
        return;
    }
    
    kinfo("Starting process in user mode: PID %d\n", process->pid);
    kinfo("  Entry point: 0x%016lx\n", process->entry_point);
    kinfo("  Stack: 0x%016lx - 0x%016lx\n", process->stack_base, process->stack_top);
    
    // Switch to process address space
    vmm_switch_address_space(process->address_space);
    
    // Set up RFLAGS for user mode
    // Enable interrupts, IOPL=0 (user mode can't do I/O)
    uint64_t rflags = 0x202;  // IF (interrupt flag) + reserved bit
    
    // Set process state to running
    process_set_state(process, PROCESS_STATE_RUNNING);
    process_set_current(process);
    
    // Calculate initial stack pointer (top of stack, aligned)
    vaddr_t user_stack = process->stack_top;
    user_stack = user_stack & ~15ULL;  // 16-byte alignment for x86-64
    
    kinfo("Switching to user mode...\n");
    
    // Call assembly function to switch to user mode
    // This will not return (unless there's an error)
    enter_user_mode(process->entry_point, user_stack, rflags);
    
    // Should never reach here
    kerror("User mode: Returned from enter_user_mode (should not happen)\n");
}

/**
 * Prepare user stack for program arguments
 * 
 * This sets up the initial stack with argc, argv, and envp.
 */
int process_setup_user_stack(process_t* process, int argc, const char** argv, const char** envp) {
    if (!process) {
        return -1;
    }
    
    // TODO: Implement stack setup with arguments
    // For now, we'll just use an empty stack
    // In a full implementation, we'd:
    // 1. Push environment variables
    // 2. Push argument strings
    // 3. Push argv array
    // 4. Push argc
    // 5. Set up auxv (auxiliary vector) for ELF
    
    kinfo("User stack prepared (simplified - no arguments yet)\n");
    return 0;
}

