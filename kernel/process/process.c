/**
 * @file process.c
 * @brief Process management implementation
 * 
 * Process creation, destruction, and management for userspace programs.
 */

#include "../include/types.h"
#include "../include/process.h"
#include "../include/mm/vmm.h"
#include "../include/mm/pmm.h"
#include "../include/mm/heap.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/errors.h"

// Process list
static process_t* process_list = NULL;
static process_t* current_process = NULL;
static pid_t next_pid = 1;
static const pid_t MAX_PID = 32767;

/**
 * Initialize process management
 */
void process_init(void) {
    kinfo("Initializing process management...\n");
    
    // Create kernel process (PID 0)
    // This is a special process that represents the kernel itself
    process_list = NULL;
    current_process = NULL;
    next_pid = 1;
    
    kinfo("Process management initialized\n");
}

/**
 * Allocate a new PID
 */
pid_t process_alloc_pid(void) {
    // Simple linear search for free PID
    // TODO: Optimize with bitmap or hash table
    for (pid_t pid = next_pid; pid <= MAX_PID; pid++) {
        bool found = false;
        for (process_t* p = process_list; p != NULL; p = p->next) {
            if (p->pid == pid) {
                found = true;
                break;
            }
        }
        if (!found) {
            next_pid = pid + 1;
            return pid;
        }
    }
    
    // Wrap around
    for (pid_t pid = 1; pid < next_pid; pid++) {
        bool found = false;
        for (process_t* p = process_list; p != NULL; p = p->next) {
            if (p->pid == pid) {
                found = true;
                break;
            }
        }
        if (!found) {
            next_pid = pid + 1;
            return pid;
        }
    }
    
    // No free PID
    return -1;
}

/**
 * Free a PID (called when process is destroyed)
 */
void process_free_pid(pid_t pid) {
    // PID will be reused automatically by process_alloc_pid
    (void)pid;  // Unused for now
}

/**
 * Create a new process
 */
process_t* process_create(const char* name, vaddr_t entry_point) {
    kinfo("Creating process: %s (entry: 0x%016lx)\n", name, entry_point);
    
    // Allocate process structure
    // TODO: Use kmalloc once heap is working
    // For now, use static allocation (temporary)
    static process_t processes[16];
    static int process_count = 0;
    
    if (process_count >= 16) {
        kerror("Process: Too many processes (max 16 for now)\n");
        return NULL;
    }
    
    process_t* process = &processes[process_count++];
    
    // Allocate PID
    pid_t pid = process_alloc_pid();
    if (pid < 0) {
        kerror("Process: No free PIDs available\n");
        return NULL;
    }
    
    // Initialize process structure
    process->pid = pid;
    process->ppid = current_process ? current_process->pid : 0;
    process->state = PROCESS_STATE_NEW;
    
    // Create address space
    process->address_space = vmm_create_address_space();
    if (!process->address_space) {
        kerror("Process: Failed to create address space\n");
        process_free_pid(pid);
        return NULL;
    }
    
    // Set up stack (8KB user stack)
    process->stack_size = 8 * 1024;
    process->stack_base = 0x00007FFFFFE00000ULL;  // User stack base
    process->stack_top = process->stack_base + process->stack_size;
    
    // Allocate and map stack pages
    size_t stack_pages = (process->stack_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < stack_pages; i++) {
        paddr_t page = pmm_alloc_page();
        if (page == 0) {
            kerror("Process: Out of memory for stack\n");
            vmm_destroy_address_space(process->address_space);
            process_free_pid(pid);
            return NULL;
        }
        
        vaddr_t stack_vaddr = process->stack_base + (i * PAGE_SIZE);
        if (vmm_map_page(process->address_space, stack_vaddr, page,
                        VMM_PRESENT | VMM_WRITE | VMM_USER | VMM_NX) != 0) {
            kerror("Process: Failed to map stack page\n");
            pmm_free_page(page);
            vmm_destroy_address_space(process->address_space);
            process_free_pid(pid);
            return NULL;
        }
    }
    
    // Set entry point
    process->entry_point = entry_point;
    process->brk = 0x0000000000400000ULL;  // Initial heap break
    
    // Initialize file descriptors (placeholder)
    process->fd_count = 0;
    process->file_descriptors = NULL;
    
    // Process tree
    process->parent = current_process;
    process->children = NULL;
    process->sibling = NULL;
    
    // Scheduling
    process->cpu_time = 0;
    process->priority = 5;  // Default priority
    
    // Exit status
    process->exit_code = 0;
    
    // Metadata
    if (name) {
        size_t name_len = 0;
        while (name[name_len] && name_len < 63) {
            process->name[name_len] = name[name_len];
            name_len++;
        }
        process->name[name_len] = '\0';
    } else {
        process->name[0] = '\0';
    }
    process->created_at = 0;  // TODO: Get timestamp
    
    // Add to process list
    process_list_add(process);
    
    // Add to parent's children list
    if (process->parent) {
        process_add_child(process->parent, process);
    }
    
    kinfo("Process created: PID %d, name: %s\n", process->pid, process->name);
    
    return process;
}

/**
 * Destroy a process
 */
void process_destroy(process_t* process) {
    if (!process) {
        return;
    }
    
    kinfo("Destroying process: PID %d\n", process->pid);
    
    // Remove from process list
    process_list_remove(process);
    
    // Remove from parent's children
    if (process->parent) {
        process_remove_child(process->parent, process);
    }
    
    // Destroy children (recursive)
    while (process->children) {
        process_destroy(process->children);
    }
    
    // Destroy address space
    if (process->address_space) {
        vmm_destroy_address_space(process->address_space);
    }
    
    // Free file descriptors (placeholder)
    // TODO: Close all file descriptors
    
    // Free PID
    process_free_pid(process->pid);
    
    // Clear process structure
    process->state = PROCESS_STATE_DEAD;
}

/**
 * Exit a process
 */
void process_exit(process_t* process, int exit_code) {
    if (!process) {
        return;
    }
    
    kinfo("Process exiting: PID %d, exit code: %d\n", process->pid, exit_code);
    
    process->exit_code = exit_code;
    process->state = PROCESS_STATE_ZOMBIE;
    
    // TODO: Notify parent process
    // TODO: Clean up resources
    // TODO: Schedule parent if it's waiting
    
    // For now, just destroy the process
    // In a real OS, we'd keep it as a zombie until parent waits
    process_destroy(process);
}

/**
 * Get process by PID
 */
process_t* process_get_by_pid(pid_t pid) {
    for (process_t* p = process_list; p != NULL; p = p->next) {
        if (p->pid == pid) {
            return p;
        }
    }
    return NULL;
}

/**
 * Get current process
 */
process_t* process_get_current(void) {
    return current_process;
}

/**
 * Get address space for a process
 */
address_space_t* process_get_address_space(process_t* process) {
    if (!process) {
        return NULL;
    }
    return process->address_space;
}

/**
 * Set current process
 */
void process_set_current(process_t* process) {
    current_process = process;
    
    // Switch address space
    if (process && process->address_space) {
        vmm_switch_address_space(process->address_space);
    }
}

/**
 * Add child to parent
 */
void process_add_child(process_t* parent, process_t* child) {
    if (!parent || !child) {
        return;
    }
    
    child->sibling = parent->children;
    parent->children = child;
}

/**
 * Remove child from parent
 */
void process_remove_child(process_t* parent, process_t* child) {
    if (!parent || !child) {
        return;
    }
    
    if (parent->children == child) {
        parent->children = child->sibling;
    } else {
        process_t* p = parent->children;
        while (p && p->sibling != child) {
            p = p->sibling;
        }
        if (p) {
            p->sibling = child->sibling;
        }
    }
    child->sibling = NULL;
}

/**
 * Set process state
 */
void process_set_state(process_t* process, process_state_t state) {
    if (process) {
        process->state = state;
    }
}

/**
 * Get process state
 */
process_state_t process_get_state(process_t* process) {
    if (process) {
        return process->state;
    }
    return PROCESS_STATE_DEAD;
}

/**
 * Get process list head
 */
process_t* process_list_head(void) {
    return process_list;
}

/**
 * Add process to list
 */
void process_list_add(process_t* process) {
    if (!process) {
        return;
    }
    
    process->next = process_list;
    process_list = process;
}

/**
 * Remove process from list
 */
void process_list_remove(process_t* process) {
    if (!process) {
        return;
    }
    
    if (process_list == process) {
        process_list = process->next;
    } else {
        process_t* p = process_list;
        while (p && p->next != process) {
            p = p->next;
        }
        if (p) {
            p->next = process->next;
        }
    }
    process->next = NULL;
}

