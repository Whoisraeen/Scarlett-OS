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
#include "../include/time.h"
#include "../include/fs/vfs.h"
#include "../include/sched/scheduler.h"

// Process list
static process_t* process_list = NULL;
static process_t* current_process = NULL;
static pid_t next_pid = 1;

// PID limits and bitmap
#define MAX_PID 32767
#define PID_BITMAP_SIZE ((MAX_PID + 63) / 64)
static uint64_t pid_bitmap[PID_BITMAP_SIZE] = {0};

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
    // Optimized PID allocation using bitmap
    // Start from next_pid for better distribution
    for (pid_t pid = next_pid; pid <= MAX_PID; pid++) {
        uint64_t bit_idx = pid / 64;
        uint64_t bit_offset = pid % 64;
        if (bit_idx < PID_BITMAP_SIZE && !(pid_bitmap[bit_idx] & (1ULL << bit_offset))) {
            // Mark as used
            pid_bitmap[bit_idx] |= (1ULL << bit_offset);
            next_pid = pid + 1;
            return pid;
        }
    }
    
    // Wrap around
    for (pid_t pid = 1; pid < next_pid; pid++) {
        uint64_t bit_idx = pid / 64;
        uint64_t bit_offset = pid % 64;
        if (bit_idx < PID_BITMAP_SIZE && !(pid_bitmap[bit_idx] & (1ULL << bit_offset))) {
            // Mark as used
            pid_bitmap[bit_idx] |= (1ULL << bit_offset);
            next_pid = pid + 1;
            return pid;
        }
    }
    
    return -1;  // No free PID
}

/**
 * Free a PID (called when process is destroyed)
 */
void process_free_pid(pid_t pid) {
    // Free PID by clearing bitmap bit
    if (pid > 0 && pid <= MAX_PID) {
        uint64_t bit_idx = pid / 64;
        uint64_t bit_offset = pid % 64;
        if (bit_idx < PID_BITMAP_SIZE) {
            pid_bitmap[bit_idx] &= ~(1ULL << bit_offset);
        }
    }
}

/**
 * Create a new process
 */
process_t* process_create(const char* name, vaddr_t entry_point) {
    kinfo("Creating process: %s (entry: 0x%016lx)\n", name, entry_point);
    
    // Allocate process structure using kmalloc
    process_t* process = (process_t*)kmalloc(sizeof(process_t));
    if (!process) {
        kerror("Process: Failed to allocate process structure\n");
        return NULL;
    }
    
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
    
    // IPC
    process->ipc_port = 0;  // Will be set when process creates IPC port
    
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
    process->created_at = time_get_uptime_ms();
    
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
    
    // Close all file descriptors
    if (process->file_descriptors && process->fd_count > 0) {
        for (int i = 0; i < process->fd_count; i++) {
            if (process->file_descriptors[i]) {
                vfs_close(i);  // Close file descriptor
            }
        }
        kfree(process->file_descriptors);
        process->file_descriptors = NULL;
        process->fd_count = 0;
    }
    
    // Free PID
    process_free_pid(process->pid);
    
    // Clear process structure
    process->state = PROCESS_STATE_DEAD;
    
    // Free process structure
    kfree(process);
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
    
    // Notify parent process if it exists
    if (process->parent) {
        // Parent will be notified when it calls wait()
        // For now, we mark the process as zombie and let parent handle cleanup
        // In a full implementation, we'd send a signal or IPC message to parent
    }
    
    // Clean up resources (but keep process structure as zombie)
    // Close file descriptors
    if (process->file_descriptors && process->fd_count > 0) {
        for (int i = 0; i < process->fd_count; i++) {
            if (process->file_descriptors[i]) {
                vfs_close(i);
            }
        }
    }
    
    // Schedule parent if it's waiting for this process
    if (process->parent) {
        // In a full implementation, we'd check if parent is blocked waiting
        // and unblock it. For now, we just mark the process as zombie.
        // The parent's wait() call will handle the actual unblocking.
    }
    
    // Keep process as zombie until parent calls wait()
    // process_destroy() will be called by parent or reaper
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

/**
 * Wait for a child process to exit
 */
pid_t process_wait(pid_t pid, int* status) {
    process_t* current = process_get_current();
    if (!current) {
        return -1;
    }
    
    while (1) {
        bool has_children = false;
        pid_t found_pid = 0;
        int exit_code = 0;
        process_t* found_child = NULL;
        
        // Iterate through children
        process_t* child = current->children;
        while (child) {
            if (pid == -1 || child->pid == pid) {
                has_children = true;
                if (child->state == PROCESS_STATE_ZOMBIE) {
                    found_pid = child->pid;
                    exit_code = child->exit_code;
                    found_child = child;
                    break;
                }
            }
            child = child->sibling;
        }
        
        if (found_child) {
            // Found a zombie child
            if (status) {
                *status = exit_code;
            }
            
            // Clean up the child process completely
            process_destroy(found_child);
            return found_pid;
        }
        
        if (!has_children) {
            // No children matching criteria
            return -1; // ECHILD
        }
        
        // Wait for children to change state
        // In a real OS, we would sleep on a wait queue
        // For now, yield and retry (busy wait with yield)
        thread_yield();
        
        // Optional: Check for signals/interrupts
    }
}

/**
 * Set process IPC port
 */
void process_set_ipc_port(process_t* process, uint64_t port_id) {
    if (process) {
        process->ipc_port = port_id;
    }
}

/**
 * Get process IPC port
 */
uint64_t process_get_ipc_port(pid_t pid) {
    process_t* process = process_get_by_pid(pid);
    if (process) {
        return process->ipc_port;
    }
    return 0;
}
