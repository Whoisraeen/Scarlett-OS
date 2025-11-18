/**
 * @file elf.c
 * @brief ELF64 loader implementation
 */

#include "elf.h"
#include "uefi.h"

/**
 * Verify ELF header
 */
static int verify_elf_header(elf64_ehdr_t* ehdr) {
    // Check magic number
    if (ehdr->e_ident[0] != 0x7F ||
        ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' ||
        ehdr->e_ident[3] != 'F') {
        return 0;
    }
    
    // Check 64-bit
    if (ehdr->e_ident[4] != ELFCLASS64) {
        return 0;
    }
    
    // Check little endian
    if (ehdr->e_ident[5] != ELFDATA2LSB) {
        return 0;
    }
    
    // Check version
    if (ehdr->e_ident[6] != EV_CURRENT) {
        return 0;
    }
    
    // Check executable
    if (ehdr->e_type != ET_EXEC) {
        return 0;
    }
    
    // Check x86_64
    if (ehdr->e_machine != EM_X86_64) {
        return 0;
    }
    
    return 1;
}

/**
 * Load ELF file into memory
 */
EFI_STATUS load_elf(void* elf_data, uint64_t* entry_point, 
                    uint64_t* kernel_start, uint64_t* kernel_end,
                    EFI_BOOT_SERVICES* bs) {
    elf64_ehdr_t* ehdr = (elf64_ehdr_t*)elf_data;
    
    // Verify ELF header
    if (!verify_elf_header(ehdr)) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Get program headers
    elf64_phdr_t* phdrs = (elf64_phdr_t*)((uint8_t*)elf_data + ehdr->e_phoff);
    
    uint64_t min_addr = 0xFFFFFFFFFFFFFFFF;
    uint64_t max_addr = 0;
    
    // Load each loadable segment
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        elf64_phdr_t* phdr = &phdrs[i];
        
        if (phdr->p_type != PT_LOAD) {
            continue;
        }
        
        // Calculate physical address
        uint64_t phys_addr = phdr->p_paddr;
        uint64_t virt_addr = phdr->p_vaddr;
        
        // Track kernel bounds
        if (phys_addr < min_addr) {
            min_addr = phys_addr;
        }
        if (phys_addr + phdr->p_memsz > max_addr) {
            max_addr = phys_addr + phdr->p_memsz;
        }
        
        // Allocate pages for segment
        uint64_t pages = (phdr->p_memsz + 0xFFF) / 0x1000;
        EFI_PHYSICAL_ADDRESS segment_addr = phys_addr;
        
        EFI_STATUS status = bs->AllocatePages(
            1, // AllocateAddress
            EfiLoaderData,
            pages,
            &segment_addr
        );
        
        if (status != EFI_SUCCESS) {
            return status;
        }
        
        // Copy segment data
        uint8_t* dest = (uint8_t*)phys_addr;
        uint8_t* src = (uint8_t*)elf_data + phdr->p_offset;
        
        // Copy file data
        for (uint64_t j = 0; j < phdr->p_filesz; j++) {
            dest[j] = src[j];
        }
        
        // Zero remaining (BSS)
        for (uint64_t j = phdr->p_filesz; j < phdr->p_memsz; j++) {
            dest[j] = 0;
        }
    }
    
    // Return information
    *entry_point = ehdr->e_entry;
    *kernel_start = min_addr;
    *kernel_end = max_addr;
    
    return EFI_SUCCESS;
}

