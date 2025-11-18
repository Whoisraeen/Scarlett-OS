/**
 * @file mmap.h
 * @brief Memory mapping interface
 */

#ifndef KERNEL_MM_MMAP_H
#define KERNEL_MM_MMAP_H

#include "../types.h"
#include "../errors.h"
#include "mm/vmm.h"

// Memory protection flags
#define PROT_READ    0x01
#define PROT_WRITE   0x02
#define PROT_EXEC    0x04
#define PROT_NONE    0x00

// Memory mapping flags
#define MAP_PRIVATE  0x01
#define MAP_SHARED   0x02
#define MAP_FIXED    0x04
#define MAP_ANONYMOUS 0x08

// Memory mapping structure
typedef struct memory_mapping {
    vaddr_t start;              // Start virtual address
    vaddr_t end;                // End virtual address
    size_t size;                // Size in bytes
    uint64_t flags;             // Protection and mapping flags
    int fd;                     // File descriptor (-1 for anonymous)
    uint64_t offset;            // File offset
    struct memory_mapping* next; // Linked list
} memory_mapping_t;

// Memory mapping functions
error_code_t mmap_init(void);
vaddr_t mmap_alloc(address_space_t* as, size_t size, uint64_t prot, uint64_t flags, int fd, uint64_t offset);
error_code_t mmap_free(address_space_t* as, vaddr_t addr, size_t size);
memory_mapping_t* mmap_find(address_space_t* as, vaddr_t addr);
error_code_t mmap_protect(address_space_t* as, vaddr_t addr, size_t size, uint64_t prot);

#endif // KERNEL_MM_MMAP_H

