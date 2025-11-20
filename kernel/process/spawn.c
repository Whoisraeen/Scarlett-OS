/**
 * @file spawn.c
 * @brief Process spawning implementation
 */

#include "../include/types.h"
#include "../include/process.h"
#include "../include/ipc/ipc.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/errors.h"
#include "../include/string.h"

/**
 * Spawn a new process
 * @param name Process name
 * @param path Path to executable (for future use, currently unused)
 * @param entry_point Entry point address
 * @return Process ID on success, -1 on error
 */
pid_t process_spawn(const char* name, const char* path, vaddr_t entry_point) {
    if (!name || entry_point == 0) {
        return -1;
    }
    
    kinfo("Spawning process: %s (entry: 0x%016lx)\n", name, entry_point);
    
    // Create new process
    process_t* process = process_create(name, entry_point);
    if (!process) {
        kerror("Spawn: Failed to create process\n");
        return -1;
    }
    
    // Create default IPC port for the process
    uint64_t port = ipc_create_port();
    if (port == 0) {
        kerror("Spawn: Failed to create IPC port\n");
        process_destroy(process);
        return -1;
    }
    
    // Store IPC port in process structure
    process->ipc_port = port;
    
    kinfo("Spawn: Created process PID %d with IPC port %lu\n", process->pid, port);
    
    return process->pid;
}

/**
 * Get IPC port of a process
 * @param pid Process ID
 * @return IPC port ID on success, 0 on error
 */
uint64_t process_get_ipc_port(pid_t pid) {
    process_t* process = process_get_by_pid(pid);
    if (!process) {
        return 0;
    }
    
    return process->ipc_port;
}

