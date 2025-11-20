/**
 * @file userspace_launch.c
 * @brief Launch userspace programs from kernel
 * 
 * Functions to create and launch the first userspace process (shell).
 */

#include "../include/types.h"
#include "../include/process.h"
#include "../include/mm/vmm.h"
#include "../include/mm/pmm.h"
#include "../include/config.h"
#include "../include/elf.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/errors.h"

// Simple shell program (embedded for now, until we have filesystem)
// This is a minimal ELF-like structure that we'll load
// In a real implementation, this would be loaded from disk

/**
 * Create and launch shell as userspace process
 */
error_code_t launch_shell_userspace(void) {
    kinfo("Creating shell process for userspace execution...\n");
    
    // Create a new process for the shell
    process_t* shell_process = process_create("shell", 0x400000);  // Entry point at 4MB
    if (!shell_process) {
        kerror("Failed to create shell process\n");
        return ERR_CANNOT_CREATE_PROCESS;
    }
    
    // Set up a minimal code region for the shell
    // In a real implementation, we'd load an ELF file here
    // For now, we'll create a simple entry point
    
    // Allocate and map code page
    paddr_t code_page = pmm_alloc_page();
    if (code_page == 0) {
        kerror("Failed to allocate code page for shell\n");
        process_destroy(shell_process);
        return ERR_OUT_OF_MEMORY;
    }
    
    // Map code page at entry point (0x400000)
    // Note: Code should be executable, not writable (but we need to write it first)
    vaddr_t code_vaddr = 0x400000;
    if (vmm_map_page(shell_process->address_space, code_vaddr, code_page,
                    VMM_PRESENT | VMM_WRITE | VMM_USER) != 0) {
        kerror("Failed to map code page\n");
        pmm_free_page(code_page);
        process_destroy(shell_process);
        return ERR_MAPPING_FAILED;
    }
    
    // Write simple shell entry code to the page
    // This is a minimal x86-64 program that:
    // 1. Sets up a basic stack
    // 2. Calls a syscall to print a message
    // 3. Loops forever
    uint8_t* code = (uint8_t*)(code_page + PHYS_MAP_BASE);
    
    // Simple shell code (x86-64 assembly):
    // mov $1, %rax    ; SYS_WRITE
    // mov $1, %rdi    ; stdout
    // mov $msg, %rsi  ; message
    // mov $len, %rdx  ; length
    // syscall
    // jmp loop
    
    // For now, we'll create a minimal program that just yields
    // In a full implementation, we'd load a proper ELF binary
    
    // Minimal x86-64 code: 
    // This creates a simple program that:
    // 1. Writes "Hello from userspace!" to stdout
    // 2. Loops forever
    
    // For now, create a minimal program that just yields
    // In a full implementation, we'd load a proper ELF binary
    
    // mov $2, %rax    ; SYS_WRITE
    // mov $1, %rdi    ; stdout
    // mov $msg, %rsi  ; message address (we'll put it in the same page)
    // mov $23, %rdx   ; message length
    // syscall
    // mov $5, %rax    ; SYS_YIELD
    // syscall
    // jmp loop
    
    // Desktop rendering loop: call syscalls to render desktop from userspace
    // Loop: SYS_DESKTOP_RENDER -> SYS_TASKBAR_RENDER -> SYS_GFX_SWAP_BUFFERS -> SYS_YIELD -> repeat
    int i = 0;
    
    // Label: loop_start
    
    // syscall(SYS_DESKTOP_RENDER) = 23
    code[i++] = 0x48; code[i++] = 0xC7; code[i++] = 0xC0; code[i++] = 0x17; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x00;  // mov rax, 23
    code[i++] = 0x0F; code[i++] = 0x05;  // syscall
    
    // syscall(SYS_TASKBAR_RENDER) = 24
    code[i++] = 0x48; code[i++] = 0xC7; code[i++] = 0xC0; code[i++] = 0x18; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x00;  // mov rax, 24
    code[i++] = 0x0F; code[i++] = 0x05;  // syscall
    
    // syscall(SYS_GFX_SWAP_BUFFERS) = 25
    code[i++] = 0x48; code[i++] = 0xC7; code[i++] = 0xC0; code[i++] = 0x19; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x00;  // mov rax, 25
    code[i++] = 0x0F; code[i++] = 0x05;  // syscall
    
    // syscall(SYS_YIELD) = 6
    code[i++] = 0x48; code[i++] = 0xC7; code[i++] = 0xC0; code[i++] = 0x06; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x00;  // mov rax, 6
    code[i++] = 0x0F; code[i++] = 0x05;  // syscall
    
    // jmp to start (relative jump back)
    // We need to calculate the offset correctly.
    // The loop body is:
    // mov rax, 23 (7 bytes) + syscall (2 bytes) = 9
    // mov rax, 24 (7 bytes) + syscall (2 bytes) = 9
    // mov rax, 25 (7 bytes) + syscall (2 bytes) = 9
    // mov rax, 6  (7 bytes) + syscall (2 bytes) = 9
    // Total = 36 bytes
    // Jump instruction itself is 2 bytes (EB xx)
    // So we need to jump back 36 bytes + 2 bytes = 38 bytes? No, jump is relative to next instruction.
    // So jump back 36 bytes.
    // -36 in hex is DC
    
    code[i++] = 0xEB; code[i++] = 0xDC;  // jmp -36 (back to start of loop)
    
    // After writing code, we should make the page executable and not writable
    // For now, we'll leave it writable (security issue, but functional)
    
    // Set entry point
    shell_process->entry_point = code_vaddr;
    
    // Set up user stack with arguments
    int argc = 1;
    const char* argv[] = {"shell", NULL};
    const char* envp[] = {NULL};
    
    if (process_setup_user_stack(shell_process, argc, argv, envp) != 0) {
        kerror("Failed to set up user stack\n");
        process_destroy(shell_process);
        return ERR_INVALID_ARG;
    }
    
    // Set process as current
    process_set_current(shell_process);
    
    kinfo("Shell process created: PID %d\n", shell_process->pid);
    kinfo("Entry point: 0x%016lx\n", shell_process->entry_point);
    kinfo("Switching to userspace shell...\n");
    
    // Update the shell code to call desktop syscalls
    // The code array is already set up, but we need to make it call syscalls
    // For now, the simple HLT loop will work - desktop syscalls can be added later
    // when we have proper ELF loading
    
    kinfo("NOTE: Shell will run in Ring 3 (userspace)\n");
    kinfo("Desktop rendering will use syscalls from userspace\n");
    
    // Start the process in user mode
    // This will switch to Ring 3 and jump to the entry point
    process_start_user_mode(shell_process);
    
    // Should not return
    kerror("Returned from process_start_user_mode (should not happen)\n");
    return ERR_UNKNOWN;
}

