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
#include "../include/auth/user.h"
#include "../include/security/audit.h"

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
    
    // Audit: Process created
    extern process_t* process_get_current(void);
    process_t* current = process_get_current();
    uint32_t uid = get_current_uid();
    uint32_t gid = get_current_gid();
    char details[256];
    strncpy(details, path ? path : "unknown", sizeof(details) - 1);
    audit_log(AUDIT_EVENT_PROCESS_CREATE, uid, gid, 
             current ? current->pid : 0, ERR_OK,
             current ? current->name : "kernel", name, "spawn", details);
    
    kinfo("Spawn: Created process PID %d with IPC port %lu\n", process->pid, port);
    
    return process->pid;
}



