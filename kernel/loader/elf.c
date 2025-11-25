/**
 * @file elf.c
 * @brief ELF loader implementation
 * 
 * Loads ELF64 executables into memory and prepares them for execution.
 */

#include "../include/types.h"
#include "../include/elf.h"
#include "../include/mm/vmm.h"
#include "../include/mm/pmm.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

/**
 * Validate ELF header
 */
int elf_validate_header(const elf64_header_t* header) {
    if (!header) {
        kerror("ELF: Null header\n");
        return -1;
    }
    
    // Check magic number
    if (header->e_ident[0] != 0x7F ||
        header->e_ident[1] != 'E' ||
        header->e_ident[2] != 'L' ||
        header->e_ident[3] != 'F') {
        kerror("ELF: Invalid magic number\n");
        return -1;
    }
    
    // Check class (64-bit)
    if (header->e_ident[4] != 2) {  // ELFCLASS64
        kerror("ELF: Not a 64-bit ELF file\n");
        return -1;
    }
    
    // Check endianness (little-endian)
    if (header->e_ident[5] != 1) {  // ELFDATA2LSB
        kerror("ELF: Not little-endian\n");
        return -1;
    }
    
    // Check version
    if (header->e_ident[6] != 1) {  // EV_CURRENT
        kerror("ELF: Invalid version\n");
        return -1;
    }
    
    // Check machine type (x86-64)
    if (header->e_machine != EM_X86_64) {
        kerror("ELF: Not x86-64 (machine: %u)\n", header->e_machine);
        return -1;
    }
    
    // Check file type (executable)
    if (header->e_type != ET_EXEC && header->e_type != ET_DYN) {
        kerror("ELF: Not an executable or shared object (type: %u)\n", header->e_type);
        return -1;
    }
    
    kinfo("ELF: Valid ELF64 executable\n");
    return 0;
}

/**
 * Load ELF segments into address space
 */
int elf_load_segments(const elf64_header_t* header, void* file_data,
                     address_space_t* address_space) {
    if (!header || !file_data || !address_space) {
        kerror("ELF: Invalid parameters\n");
        return -1;
    }
    
    kinfo("ELF: Loading %u segments...\n", header->e_phnum);
    
    // Get program headers
    const elf64_program_header_t* phdr = (const elf64_program_header_t*)
        ((uint8_t*)file_data + header->e_phoff);
    
    // Load each loadable segment
    for (uint16_t i = 0; i < header->e_phnum; i++) {
        const elf64_program_header_t* ph = &phdr[i];
        
        if (ph->p_type != PT_LOAD) {
            continue;  // Skip non-loadable segments
        }
        
        kinfo("ELF: Loading segment %u: vaddr=0x%016lx, size=%lu bytes\n",
              i, ph->p_vaddr, ph->p_memsz);
        
        // Calculate number of pages needed
        vaddr_t seg_start = ALIGN_DOWN(ph->p_vaddr, PAGE_SIZE);
        vaddr_t seg_end = ALIGN_UP(ph->p_vaddr + ph->p_memsz, PAGE_SIZE);
        size_t pages = (seg_end - seg_start) / PAGE_SIZE;
        
        kinfo("ELF: Mapping %lu pages (0x%016lx - 0x%016lx)\n",
              pages, seg_start, seg_end);
        
        // Map pages for this segment
        for (size_t j = 0; j < pages; j++) {
            vaddr_t vaddr = seg_start + (j * PAGE_SIZE);
            
            // Allocate physical page
            paddr_t paddr = pmm_alloc_page();
            if (paddr == 0) {
                kerror("ELF: Out of memory for segment\n");
                return -1;
            }
            
            // Determine flags
            uint64_t flags = VMM_PRESENT | VMM_USER;
            if (ph->p_flags & PF_W) {
                flags |= VMM_WRITE;
            }
            if (!(ph->p_flags & PF_X)) {
                flags |= VMM_NX;  // No-execute if not executable
            }
            
            // Map page
            if (vmm_map_page(address_space, vaddr, paddr, flags) != 0) {
                kerror("ELF: Failed to map page at 0x%016lx\n", vaddr);
                pmm_free_page(paddr);
                return -1;
            }
            
            // Clear page (zero-initialize)
            uint8_t* page_virt = (uint8_t*)(paddr + 0xFFFF800000000000ULL);  // Use identity mapping for now
            for (size_t k = 0; k < PAGE_SIZE; k++) {
                page_virt[k] = 0;
            }
        }
        
        // Copy segment data from file
        if (ph->p_filesz > 0) {
            vaddr_t load_vaddr = ph->p_vaddr;
            const uint8_t* file_data_ptr = (const uint8_t*)file_data + ph->p_offset;
            
            kinfo("ELF: Copying %lu bytes to 0x%016lx\n", ph->p_filesz, load_vaddr);
            
            // Copy data page by page
            for (size_t offset = 0; offset < ph->p_filesz; offset += PAGE_SIZE) {
                vaddr_t page_vaddr = load_vaddr + offset;
                size_t copy_size = (ph->p_filesz - offset < PAGE_SIZE) ? 
                                   (ph->p_filesz - offset) : PAGE_SIZE;
                
                // Copy data using proper virtual address access
                // DONE: Virtual address access implemented
                // Use the virtual address directly (page is already mapped in the address space)
                uint8_t* dest = (uint8_t*)page_vaddr;  // Use virtual address directly
                const uint8_t* src = file_data_ptr + offset;
                
                for (size_t k = 0; k < copy_size; k++) {
                    dest[k] = src[k];
                }
            }
            
            kinfo("ELF: Segment data copied\n");
        }
        
        kinfo("ELF: Segment %u loaded successfully\n", i);
    }
    
    kinfo("ELF: All segments loaded\n");
    return 0;
}

/**
 * Get entry point from ELF header
 */
vaddr_t elf_get_entry_point(const elf64_header_t* header) {
    if (!header) {
        return 0;
    }
    return header->e_entry;
}

/**
 * Load ELF executable from memory
 * 
 * This is the main function to load an ELF executable.
 * It validates the header, loads segments, and returns the entry point.
 */
int elf_load_executable(void* file_data, size_t file_size,
                       address_space_t* address_space,
                       vaddr_t* entry_point) {
    if (!file_data || file_size < sizeof(elf64_header_t)) {
        kerror("ELF: Invalid file data\n");
        return -1;
    }
    
    const elf64_header_t* header = (const elf64_header_t*)file_data;
    
    // Validate header
    if (elf_validate_header(header) != 0) {
        return -1;
    }
    
    // Check file size
    if (file_size < header->e_phoff + 
        (header->e_phnum * sizeof(elf64_program_header_t))) {
        kerror("ELF: File too small for program headers\n");
        return -1;
    }
    
    // Load segments
    if (elf_load_segments(header, file_data, address_space) != 0) {
        return -1;
    }
    
    // Get entry point
    if (entry_point) {
        *entry_point = elf_get_entry_point(header);
        kinfo("ELF: Entry point: 0x%016lx\n", *entry_point);
    }
    
    return 0;
}

