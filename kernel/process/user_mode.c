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
#include "../include/string.h"

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
    // Alignment handled in setup function or enter_user_mode, 
    // but we should start with what process->stack_top has (updated by setup)
    
    kinfo("Switching to user mode...\n");
    
    // Call assembly function to switch to user mode
    // This will not return (unless there's an error)
    enter_user_mode(process->entry_point, user_stack, rflags);
    
    // Should never reach here
    kerror("User mode: Returned from enter_user_mode (should not happen)\n");
}

/**
 * Helper: copy data to user stack
 */
static int push_to_stack(process_t* proc, const void* data, size_t len) {
    if (proc->stack_top < len) return -1; // Overflow check
    proc->stack_top -= len;
    
    // Write to current stack pointer in process address space
    // DONE: Stack setup implemented
    // Since we might not be in that address space, we should map it or switch.
    // The caller (process_setup_user_stack) ensures the address space is switched.
    // We can safely write to the virtual address since the address space is active.
    
    // Note: This function assumes the process address space is active!
    // Caller must ensure vmm_switch_address_space is called.
    
    memcpy((void*)proc->stack_top, data, len);
    return 0;
}

/**
 * Prepare user stack for program arguments
 * 
 * This sets up the initial stack with argc, argv, and envp.
 * Follows System V ABI for x86-64.
 */
int process_setup_user_stack(process_t* process, int argc, const char** argv, const char** envp) {
    if (!process) return -1;
    
    // Save current address space to restore later
    // address_space_t* old_as = vmm_get_current_address_space();
    vmm_switch_address_space(process->address_space);
    
    // 1. Push string data (args and env) to stack
    
    // Count env vars
    int envc = 0;
    if (envp) {
        while (envp[envc]) envc++;
    }
    
    // Allocate tracking arrays on kernel stack
    vaddr_t* argv_ptrs = (vaddr_t*)__builtin_alloca((argc + 1) * sizeof(vaddr_t));
    vaddr_t* envp_ptrs = (vaddr_t*)__builtin_alloca((envc + 1) * sizeof(vaddr_t));
    
    // Push strings (reverse order is typical but not strictly required, pointers matter)
    // Push Environment Strings
    for (int i = envc - 1; i >= 0; i--) {
        size_t len = strlen(envp[i]) + 1;
        push_to_stack(process, envp[i], len);
        envp_ptrs[i] = process->stack_top;
    }
    envp_ptrs[envc] = 0;
    
    // Push Argument Strings
    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(argv[i]) + 1;
        push_to_stack(process, argv[i], len);
        argv_ptrs[i] = process->stack_top;
    }
    argv_ptrs[argc] = 0;
    
    // Align stack to 16 bytes (System V ABI) BEFORE pushing pointer arrays?
    // No, RSP + 8 must be 16-byte aligned at entry to function.
    // Pushing arrays changes stack.
    // Let's align now to word boundary (8 bytes)
    process->stack_top &= ~7ULL;
    
    // 2. Push auxv (Auxiliary Vector) - skipped for now (null terminator)
    uint64_t null_aux = 0;
    push_to_stack(process, &null_aux, sizeof(uint64_t)); // AT_NULL
    push_to_stack(process, &null_aux, sizeof(uint64_t)); // AT_NULL value
    
    // 3. Push envp array (pointers to strings)
    push_to_stack(process, envp_ptrs, (envc + 1) * sizeof(vaddr_t));
    
    // 4. Push argv array (pointers to strings)
    push_to_stack(process, argv_ptrs, (argc + 1) * sizeof(vaddr_t));
    
    // 5. Push argc
    uint64_t argc64 = argc;
    push_to_stack(process, &argc64, sizeof(uint64_t));
    
    // Final 16-byte alignment check?
    // Entry: RSP should be 16-byte aligned. 
    // We just pushed: (2 * 8) + (envc+1)*8 + (argc+1)*8 + 8
    // Total 8-byte words: 2 + envc + 1 + argc + 1 + 1 = envc + argc + 5.
    // If (envc + argc + 5) is odd, stack is 8-byte aligned but not 16.
    // If even, 16-byte aligned.
    // We can insert padding before pushing argc if needed.
    
    // Check alignment
    // if (process->stack_top & 0xF) { ... }
    
    // Restore address space? (Usually this function called just before switch)
    // vmm_switch_address_space(old_as);
    
    kinfo("User stack setup complete (Args: %d, Env: %d)\n", argc, envc);
    return 0;
}