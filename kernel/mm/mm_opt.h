/**
 * @file mm_opt.h
 * @brief Memory Management Optimizations
 *
 * Per-CPU page caches, slab allocator improvements, and huge page support
 */

#ifndef KERNEL_MM_OPT_H
#define KERNEL_MM_OPT_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_ORDER 11
#define PAGE_SIZE 4096
#define HUGE_PAGE_SIZE (2 * 1024 * 1024)  // 2MB

// Per-CPU page cache
typedef struct {
    void* free_pages[MAX_ORDER];
    uint32_t count[MAX_ORDER];
    uint32_t max_count;
} percpu_page_cache_t;

// Huge page descriptor
typedef struct huge_page {
    void* vaddr;
    uint64_t paddr;
    uint32_t order;
    bool allocated;
    struct huge_page* next;
} huge_page_t;

// Memory statistics
typedef struct {
    uint64_t total_pages;
    uint64_t free_pages;
    uint64_t cached_pages;
    uint64_t huge_pages;
    uint64_t allocations;
    uint64_t deallocations;
    uint64_t cache_hits;
    uint64_t cache_misses;
} mm_stats_t;

// Initialize optimized memory management
int mm_opt_init(void);
void mm_opt_cleanup(void);

// Per-CPU page cache
int mm_percpu_cache_init(uint32_t cpu);
void* mm_percpu_alloc_page(uint32_t cpu, uint32_t order);
void mm_percpu_free_page(uint32_t cpu, void* page, uint32_t order);

// Huge page support
int mm_hugepage_init(void);
void* mm_alloc_hugepage(void);
void mm_free_hugepage(void* addr);
bool mm_is_hugepage(void* addr);

// Memory compaction
void mm_compact_memory(void);
void mm_compact_zone(uint32_t zone_id);

// Statistics
void mm_get_stats(mm_stats_t* stats);
void mm_print_stats(void);

#endif // KERNEL_MM_OPT_H
