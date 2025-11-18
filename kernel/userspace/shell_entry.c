/**
 * @file shell_entry.c
 * @brief Userspace shell entry point
 * 
 * This is the entry point for the shell when running in userspace (Ring 3).
 * It uses system calls to interact with the kernel.
 */

// This will be compiled as a userspace program
// For now, we'll create a simple entry point that can be loaded

// Userspace shell entry point
void _start(void) {
    // This is a placeholder for the userspace shell
    // In a full implementation, this would:
    // 1. Call syscalls to read/write
    // 2. Implement shell logic in userspace
    // 3. Use syscalls for all kernel services
    
    // For now, we'll just loop and make syscalls
    while (1) {
        // Make a syscall to yield
        __asm__ volatile(
            "mov $5, %%rax\n"  // SYS_YIELD
            "syscall\n"
            :
            :
            : "rax", "rcx", "r11"
        );
    }
}

