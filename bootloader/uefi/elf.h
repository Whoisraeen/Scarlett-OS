/**
 * @file elf.h
 * @brief ELF64 format definitions
 */

#ifndef ELF_H
#define ELF_H

#include <stdint.h>

// ELF magic number
#define ELF_MAGIC 0x464C457F  // "\x7FELF"

// ELF class
#define ELFCLASS64 2

// ELF data encoding
#define ELFDATA2LSB 1

// ELF version
#define EV_CURRENT 1

// ELF types
#define ET_EXEC 2

// ELF machine
#define EM_X86_64 62

// Program header types
#define PT_LOAD 1

// Program header flags
#define PF_X 0x1  // Execute
#define PF_W 0x2  // Write
#define PF_R 0x4  // Read

// ELF header
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf64_ehdr_t;

// Program header
typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) elf64_phdr_t;

#endif // ELF_H

