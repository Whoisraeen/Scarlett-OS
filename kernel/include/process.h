/**
 * @file process.h
 * @brief Process management interface
 * 
 * Process management for userspace programs.
 */

#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include "types.h"
#include "errors.h"
#include "mm/vmm.h"

// Process states
typedef enum {
    PROCESS_STATE_NEW,        // Just created
    PROCESS_STATE_RUNNING,     // Currently executing
    PROCESS_STATE_BLOCKED,     // Waiting for I/O or event
    PROCESS_STATE_ZOMBIE,      // Terminated but not cleaned up
    PROCESS_STATE_DEAD         // Fully terminated
} process_state_t;

// Process structure
typedef struct process {
    // Identification
    pid_t pid;                  // Process ID
    pid_t ppid;                 // Parent process ID
    
    // State
    process_state_t state;      // Current state
    
    // Memory
    address_space_t* address_space;  // Virtual address space
    vaddr_t stack_base;         // User stack base
    vaddr_t stack_top;          // User stack top
    size_t stack_size;          // Stack size
    
    // Execution
    vaddr_t entry_point;        // Program entry point
    vaddr_t brk;                // Program break (heap end)
    
    // File descriptors (placeholder for now)
    int fd_count;
    void** file_descriptors;    // Array of file descriptors
    
    // Process tree
    struct process* parent;     // Parent process
    struct process* children;   // First child
    struct process* sibling;    // Next sibling
    
    // Scheduling
    uint64_t cpu_time;          // CPU time used
    uint32_t priority;          // Process priority
    
    // Exit status
    int exit_code;              // Exit code (if terminated)
    
    // Metadata
    char name[64];              // Process name
    uint64_t created_at;        // Creation timestamp
    
    // IPC
    uint64_t ipc_port;          // Default IPC port for this process
    
    // Security
    uint32_t uid;               // User ID
    uint32_t gid;               // Group ID
    
    // Linked list
    struct process* next;       // Next process in list
} process_t;

// Process management functions
process_t* process_create(const char* name, vaddr_t entry_point);
void process_destroy(process_t* process);
void process_exit(process_t* process, int exit_code);

// Process lookup
process_t* process_get_by_pid(pid_t pid);
process_t* process_get_current(void);
void process_set_current(process_t* process);

// Process tree
void process_add_child(process_t* parent, process_t* child);
void process_remove_child(process_t* parent, process_t* child);

// PID management
pid_t process_alloc_pid(void);
void process_free_pid(pid_t pid);

// Process state
void process_set_state(process_t* process, process_state_t state);
process_state_t process_get_state(process_t* process);

// Process list
process_t* process_list_head(void);
void process_list_add(process_t* process);
void process_list_remove(process_t* process);

// Initialization
void process_init(void);

// User mode execution
void process_start_user_mode(process_t* process);
int process_setup_user_stack(process_t* process, int argc, const char** argv, const char** envp);

// Address space access
address_space_t* process_get_address_space(process_t* process);

// Process operations
pid_t process_fork(process_t* parent);
error_code_t process_exec(process_t* process, const char* path, char* const* argv, char* const* envp);

// Process spawning
pid_t process_spawn(const char* name, const char* path, vaddr_t entry_point);
uint64_t process_get_ipc_port(pid_t pid);

#endif // KERNEL_PROCESS_H

