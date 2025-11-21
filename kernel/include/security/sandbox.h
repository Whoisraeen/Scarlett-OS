/**
 * @file sandbox.h
 * @brief Sandboxing support interface
 */

#ifndef KERNEL_SECURITY_SANDBOX_H
#define KERNEL_SECURITY_SANDBOX_H

#include "../types.h"
#include "../errors.h"

// Resource limits
#define SANDBOX_MAX_MEMORY      (512 * 1024 * 1024)  // 512 MB
#define SANDBOX_MAX_FILES       64
#define SANDBOX_MAX_PROCESSES   8
#define SANDBOX_MAX_FDS         128

// Sandbox flags
#define SANDBOX_FLAG_NETWORK    0x01
#define SANDBOX_FLAG_FILESYSTEM 0x02
#define SANDBOX_FLAG_DEVICE     0x04
#define SANDBOX_FLAG_IPC        0x08

// Sandbox resource limits
typedef struct {
    size_t max_memory;        // Maximum memory (bytes)
    size_t max_files;          // Maximum open files
    uint32_t max_processes;    // Maximum child processes
    uint32_t max_fds;          // Maximum file descriptors
    uint32_t flags;            // Sandbox flags
} sandbox_limits_t;

// Sandbox structure
typedef struct {
    uint32_t pid;              // Process ID
    sandbox_limits_t limits;   // Resource limits
    size_t current_memory;     // Current memory usage
    uint32_t current_files;    // Current open files
    uint32_t current_processes;// Current child processes
    uint32_t current_fds;      // Current file descriptors
    bool active;               // Is sandbox active?
} sandbox_t;

// Sandbox functions
error_code_t sandbox_init(void);
error_code_t sandbox_create(pid_t pid, const sandbox_limits_t* limits);
error_code_t sandbox_destroy(pid_t pid);
sandbox_t* sandbox_get(pid_t pid);
error_code_t sandbox_check_memory(pid_t pid, size_t requested);
error_code_t sandbox_check_file(pid_t pid);
error_code_t sandbox_check_process(pid_t pid);
error_code_t sandbox_check_fd(pid_t pid);
error_code_t sandbox_check_flag(pid_t pid, uint32_t flag);
error_code_t sandbox_update_memory(pid_t pid, size_t delta);
error_code_t sandbox_update_files(pid_t pid, int delta);
error_code_t sandbox_update_processes(pid_t pid, int delta);
error_code_t sandbox_update_fds(pid_t pid, int delta);

#endif // KERNEL_SECURITY_SANDBOX_H

