/**
 * @file paging.h
 * @brief Page table setup for x86_64
 */

#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include "uefi.h"

// Page table entry flags
#define PAGE_PRESENT    (1ULL << 0)
#define PAGE_WRITE      (1ULL << 1)
#define PAGE_USER       (1ULL << 2)
#define PAGE_WRITETHROUGH (1ULL << 3)
#define PAGE_NOCACHE    (1ULL << 4)
#define PAGE_ACCESSED   (1ULL << 5)
#define PAGE_DIRTY      (1ULL << 6)
#define PAGE_HUGE       (1ULL << 7)
#define PAGE_GLOBAL     (1ULL << 8)
#define PAGE_NX         (1ULL << 63)

/**
 * Set up page tables for kernel
 */
EFI_STATUS setup_page_tables(uint64_t* pml4_addr, 
                              uint64_t kernel_phys_start,
                              uint64_t kernel_phys_end,
                              uint64_t framebuffer_addr,
                              uint64_t framebuffer_size,
                              EFI_BOOT_SERVICES* bs);

#endif // PAGING_H

