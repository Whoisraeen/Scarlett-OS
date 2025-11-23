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
#include "../include/fs/vfs.h"

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
    
    // TODO: Load ELF file from filesystem - DONE: ELF loading from filesystem implemented
    // 1. Open the file
    extern error_code_t vfs_open(const char* path, uint64_t flags, fd_t* fd);
    extern error_code_t vfs_read(fd_t fd, void* buf, size_t count, size_t* bytes_read);
    extern error_code_t vfs_close(fd_t fd);
    
    fd_t file_fd;
    error_code_t err = vfs_open(path, VFS_MODE_READ, &file_fd);
    if (err != ERR_OK) {
        kerror("Exec: Failed to open file %s: %d\n", path, err);
        return err;
    }
    
    // 2. Read ELF header
    elf64_header_t header;
    size_t bytes_read = 0;
    err = vfs_read(file_fd, &header, sizeof(elf64_header_t), &bytes_read);
    if (err != ERR_OK || bytes_read != sizeof(elf64_header_t)) {
        kerror("Exec: Failed to read ELF header\n");
        vfs_close(file_fd);
        return err != ERR_OK ? err : ERR_INVALID_STATE;
    }
    
    // Validate ELF header
    extern int elf_validate_header(const elf64_header_t* header);
    if (elf_validate_header(&header) != 0) {
        kerror("Exec: Invalid ELF file\n");
        vfs_close(file_fd);
        return ERR_INVALID_STATE;
    }
    
    // Get file size (read until EOF or use stat)
    // For now, read the entire file into memory
    // TODO: Optimize to read segments on-demand
    size_t file_size = 0;
    size_t buffer_size = 1024 * 1024;  // 1MB buffer (should be enough for most programs)
    void* file_buffer = kmalloc(buffer_size);
    if (!file_buffer) {
        kerror("Exec: Failed to allocate file buffer\n");
        vfs_close(file_fd);
        return ERR_OUT_OF_MEMORY;
    }
    
    // Read entire file
    size_t total_read = 0;
    while (total_read < buffer_size) {
        size_t chunk_read = 0;
        err = vfs_read(file_fd, (uint8_t*)file_buffer + total_read, 
                      buffer_size - total_read, &chunk_read);
        if (err != ERR_OK || chunk_read == 0) {
            break;
        }
        total_read += chunk_read;
    }
    file_size = total_read;
    
    vfs_close(file_fd);
    
    // 3. Load segments into new address space
    vaddr_t entry_point = 0;
    extern int elf_load_executable(void* file_data, size_t file_size,
                                   address_space_t* address_space,
                                   vaddr_t* entry_point);
    
    if (elf_load_executable(file_buffer, file_size, process->address_space, &entry_point) != 0) {
        kerror("Exec: Failed to load ELF segments\n");
        kfree(file_buffer);
        return ERR_INVALID_STATE;
    }
    
    // Free file buffer (segments are now loaded)
    kfree(file_buffer);
    
    // 4. Set up stack with argv/envp
    extern int process_setup_user_stack(process_t* process, int argc, const char** argv, const char** envp);
    
    // Count argc
    int argc = 0;
    if (argv) {
        while (argv[argc]) argc++;
    }
    
    if (process_setup_user_stack(process, argc, argv, envp) != 0) {
        kerror("Exec: Failed to set up user stack\n");
        return ERR_INVALID_STATE;
    }
    
    // 5. Set entry point and update process
    process->entry_point = entry_point;
    
    kinfo("Exec: ELF loaded successfully, entry point: 0x%016lx\n", entry_point);
    return ERR_OK;
}

