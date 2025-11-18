/**
 * @file fork_exec.c
 * @brief Process fork and exec implementation
 */

#include "../include/types.h"
#include "../include/process.h"
#include "../include/mm/vmm.h"
#include "../include/mm/pmm.h"
#include "../include/mm/heap.h"
#include "../include/config.h"
#include "../include/elf.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/errors.h"

/**
 * Fork a process (create a copy of the current process)
 */
pid_t process_fork(process_t* parent) {
    if (!parent) {
        return -1;
    }
    
    kinfo("Forking process: PID %d\n", parent->pid);
    
    // Create new process
    process_t* child = process_create("forked", parent->entry_point);
    if (!child) {
        kerror("Fork: Failed to create child process\n");
        return -1;
    }
    
    // Copy parent's address space (copy-on-write would be better, but for now copy)
    // TODO: Implement copy-on-write
    // For now, we'll create a new address space and copy pages
    
    // Copy stack
    size_t stack_pages = (parent->stack_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < stack_pages; i++) {
        vaddr_t parent_vaddr = parent->stack_base + (i * PAGE_SIZE);
        vaddr_t child_vaddr = child->stack_base + (i * PAGE_SIZE);
        
        // Get parent's physical page
        paddr_t parent_paddr = vmm_get_physical(parent->address_space, parent_vaddr);
        if (parent_paddr == 0) {
            kerror("Fork: Failed to get parent physical page\n");
            process_destroy(child);
            return -1;
        }
        
        // Allocate new page for child
        paddr_t child_paddr = pmm_alloc_page();
        if (child_paddr == 0) {
            kerror("Fork: Out of memory\n");
            process_destroy(child);
            return -1;
        }
        
        // Map child's page (stack pages are non-executable)
        if (vmm_map_page(child->address_space, child_vaddr, child_paddr,
                        VMM_PRESENT | VMM_WRITE | VMM_USER | VMM_NX) != 0) {
            kerror("Fork: Failed to map child page\n");
            pmm_free_page(child_paddr);
            process_destroy(child);
            return -1;
        }
        
        // Copy page contents
        uint8_t* parent_data = (uint8_t*)(parent_paddr + PHYS_MAP_BASE);
        uint8_t* child_data = (uint8_t*)(child_paddr + PHYS_MAP_BASE);
        memcpy(child_data, parent_data, PAGE_SIZE);
    }
    
    // Copy other process attributes
    child->ppid = parent->pid;
    child->brk = parent->brk;
    child->priority = parent->priority;
    
    // Set up parent-child relationship
    process_add_child(parent, child);
    
    kinfo("Fork: Created child process PID %d (parent PID %d)\n", child->pid, parent->pid);
    
    return child->pid;
}

/**
 * Execute a new program (replace current process)
 */
error_code_t process_exec(process_t* process, const char* path, char* const* argv, char* const* envp) {
    if (!process || !path) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("Exec: PID %d executing %s\n", process->pid, path);
    
    // TODO: Load ELF file from filesystem
    // For now, this is a placeholder
    // In a real implementation, we would:
    // 1. Open the file
    // 2. Read ELF header
    // 3. Load segments into new address space
    // 4. Set up stack with argv/envp
    // 5. Jump to entry point
    
    kerror("Exec: Not yet implemented (filesystem needed)\n");
    return ERR_NOT_SUPPORTED;
}

