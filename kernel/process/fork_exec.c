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
    
    // Copy parent's address space using Copy-on-Write
    // Map child's pages to same physical pages as parent, marked as CoW
    
    // Copy stack using CoW
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
        
        // Increment reference count for the physical page
        extern void pmm_ref_page(paddr_t page);
        pmm_ref_page(parent_paddr);
        
        // Map child's page to same physical page, but mark as CoW (no write permission)
        // Stack pages are non-executable
        uint64_t flags = VMM_PRESENT | VMM_USER | VMM_NX | VMM_COW;
        if (vmm_map_page(child->address_space, child_vaddr, parent_paddr, flags) != 0) {
            kerror("Fork: Failed to map child page\n");
            pmm_free_page(parent_paddr);  // Decrement ref count
            process_destroy(child);
            return -1;
        }
        
        // Mark parent's page as CoW too (if not already)
        extern int vmm_mark_cow(address_space_t* as, vaddr_t vaddr);
        vmm_mark_cow(parent->address_space, parent_vaddr);
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

