/**
 * @file elf.h
 * @brief ELF (Executable and Linkable Format) structures
 * 
 * ELF64 structures for loading and executing programs.
 */

#ifndef KERNEL_ELF_H
#define KERNEL_ELF_H

#include "../types.h"

// ELF magic number
#define ELF_MAGIC 0x464C457F  // "\x7FELF"

// ELF file types
#define ET_NONE   0  // No file type
#define ET_REL    1  // Relocatable file
#define ET_EXEC   2  // Executable file
#define ET_DYN    3  // Shared object file
#define ET_CORE   4  // Core file

// ELF machine types
#define EM_X86_64 62  // x86-64

// ELF header
typedef struct {
    uint8_t  e_ident[16];     // ELF identification
    uint16_t e_type;          // Object file type
    uint16_t e_machine;       // Machine type
    uint32_t e_version;       // Object file version
    uint64_t e_entry;         // Entry point virtual address
    uint64_t e_phoff;         // Program header table file offset
    uint64_t e_shoff;         // Section header table file offset
    uint32_t e_flags;        // Processor-specific flags
    uint16_t e_ehsize;        // ELF header size
    uint16_t e_phentsize;     // Program header entry size
    uint16_t e_phnum;         // Number of program header entries
    uint16_t e_shentsize;     // Section header entry size
    uint16_t e_shnum;         // Number of section header entries
    uint16_t e_shstrndx;      // Section header string table index
} __attribute__((packed)) elf64_header_t;

// Program header types
#define PT_NULL    0  // Unused
#define PT_LOAD    1  // Loadable segment
#define PT_DYNAMIC 2  // Dynamic linking information
#define PT_INTERP 3   // Interpreter pathname
#define PT_NOTE    4  // Auxiliary information
#define PT_SHLIB  5   // Reserved
#define PT_PHDR   6   // Program header table

// Program header flags
#define PF_X (1 << 0)  // Execute
#define PF_W (1 << 1)  // Write
#define PF_R (1 << 2)  // Read

// Program header
typedef struct {
    uint32_t p_type;    // Segment type
    uint32_t p_flags;   // Segment flags
    uint64_t p_offset;  // Segment file offset
    uint64_t p_vaddr;   // Segment virtual address
    uint64_t p_paddr;   // Segment physical address
    uint64_t p_filesz;  // Segment size in file
    uint64_t p_memsz;   // Segment size in memory
    uint64_t p_align;   // Segment alignment
} __attribute__((packed)) elf64_program_header_t;

// ELF loading functions
int elf_validate_header(const elf64_header_t* header);
int elf_load_segments(const elf64_header_t* header, void* file_data, 
                     address_space_t* address_space);
vaddr_t elf_get_entry_point(const elf64_header_t* header);

#endif // KERNEL_ELF_H

