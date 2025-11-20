/**
 * @file heap.c
 * @brief Kernel heap allocator implementation
 *
 * Simple first-fit allocator with coalescing.
 * Good enough for Phase 2, will be replaced with slab allocator later.
 */

#include "../include/types.h"
#include "../include/config.h"
#include "../include/mm/heap.h"
#include "../include/mm/slab.h"
#include "../include/mm/pmm.h"
#include "../include/mm/vmm.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/sync/spinlock.h"

// Block header
typedef struct heap_block {
    size_t size;                    // Size of data area
    bool free;                      // Is this block free?
    struct heap_block* next;        // Next block
    struct heap_block* prev;        // Previous block
    uint32_t magic;                 // Magic number for validation
} heap_block_t;

#define HEAP_MAGIC 0xDEADBEEF
#define BLOCK_HEADER_SIZE sizeof(heap_block_t)

// Heap state
static heap_block_t* heap_start = NULL;
static vaddr_t heap_current = HEAP_START;
static vaddr_t heap_max = HEAP_START;
static size_t heap_total_size = 0;
static size_t heap_used_size = 0;

/**
 * Expand heap by allocating more pages
 */
static int expand_heap(size_t needed_size) {
    size_t size = ALIGN_UP(needed_size, PAGE_SIZE);

    if (heap_current + size > HEAP_START + HEAP_MAX_SIZE) {
        kerror("Heap: Cannot expand beyond maximum size\n");
        return -1;
    }

    size_t pages = size / PAGE_SIZE;

    for (size_t i = 0; i < pages; i++) {
        paddr_t page = pmm_alloc_page();
        if (page == 0) {
            kerror("Heap: Out of physical memory at page %lu/%lu\n", i, pages);
            return -1;
        }

        // Map page to kernel address space (NULL = kernel AS)
        // This will create page tables if needed - VMM should use low memory for page tables
        // since PHYS_MAP_BASE might not be fully ready yet
        kdebug("Heap: Mapping page %lu/%lu: virt=0x%016lx -> phys=0x%016lx...\n", 
               i + 1, pages, heap_current, page);
        int result = vmm_map_page(NULL, heap_current, page,
                                 VMM_PRESENT | VMM_WRITE | VMM_NX);
        if (result != 0) {
            kerror("Heap: Failed to map page at 0x%016lx (physical 0x%016lx, result=%d)\n",
                   heap_current, page, result);
            pmm_free_page(page);
            return -1;
        }
        kdebug("Heap: Page %lu/%lu mapped successfully\n", i + 1, pages);

        heap_current += PAGE_SIZE;
    }

    heap_max = heap_current;
    heap_total_size += size;
    return 0;
}

/**
 * Coalesce adjacent free blocks
 */
static void coalesce_free_blocks(void) {
    if (!heap_start) return;
    
    heap_block_t* block = heap_start;
    
    while (block && block->next) {
        if (block->free && block->next->free) {
            // Merge with next block
            block->size += BLOCK_HEADER_SIZE + block->next->size;
            block->next = block->next->next;
            
            if (block->next) {
                block->next->prev = block;
            }
        } else {
            block = block->next;
        }
    }
}

/**
 * Initialize kernel heap
 */
void heap_init(void) {
    kinfo("Initializing kernel heap...\n");

    // Initialize slab allocator first
    extern void slab_init(void);
    slab_init();

    // Expand heap initially (vmm_map_page with NULL uses kernel address space)
    if (expand_heap(HEAP_INITIAL_SIZE) != 0) {
        kerror("Heap: Failed to expand heap\n");
        kpanic("Failed to initialize heap");
    }

    // Create first block
    heap_start = (heap_block_t*)HEAP_START;
    heap_start->size = HEAP_INITIAL_SIZE - BLOCK_HEADER_SIZE;
    heap_start->free = true;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    heap_start->magic = HEAP_MAGIC;

    kinfo("Heap initialized: %lu MB\n", heap_total_size / (1024 * 1024));
}

/**
 * Allocate memory from heap
 */
void* kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    // Use slab allocator for small objects (<= 4KB)
    if (size <= 4096) {
        extern void* slab_alloc(size_t size);
        void* ptr = slab_alloc(size);
        if (ptr) {
            return ptr;
        }
        // Fall through to regular heap if slab allocation fails
    }
    
    // Align size
    size = ALIGN_UP(size, 8);
    
    // Find free block (first-fit)
    heap_block_t* block = heap_start;
    
    while (block) {
        if (block->free && block->size >= size) {
            // Found suitable block
            
            // Split block if too large
            if (block->size >= size + BLOCK_HEADER_SIZE + 32) {
                heap_block_t* new_block = (heap_block_t*)((uint8_t*)block + BLOCK_HEADER_SIZE + size);
                new_block->size = block->size - size - BLOCK_HEADER_SIZE;
                new_block->free = true;
                new_block->next = block->next;
                new_block->prev = block;
                new_block->magic = HEAP_MAGIC;
                
                if (block->next) {
                    block->next->prev = new_block;
                }
                
                block->next = new_block;
                block->size = size;
            }
            
            block->free = false;
            heap_used_size += block->size + BLOCK_HEADER_SIZE;
            
            return (void*)((uint8_t*)block + BLOCK_HEADER_SIZE);
        }
        
        block = block->next;
    }
    
    // No suitable block found, need to expand heap
    size_t expand_size = size + BLOCK_HEADER_SIZE;
    if (expand_heap(expand_size) != 0) {
        return NULL;
    }
    
    // Create new block at end
    heap_block_t* last_block = heap_start;
    while (last_block->next) {
        last_block = last_block->next;
    }
    
    heap_block_t* new_block = (heap_block_t*)((uint8_t*)last_block + BLOCK_HEADER_SIZE + last_block->size);
    new_block->size = expand_size - BLOCK_HEADER_SIZE;
    new_block->free = false;
    new_block->next = NULL;
    new_block->prev = last_block;
    new_block->magic = HEAP_MAGIC;
    
    last_block->next = new_block;
    heap_used_size += new_block->size + BLOCK_HEADER_SIZE;
    
    return (void*)((uint8_t*)new_block + BLOCK_HEADER_SIZE);
}

/**
 * Allocate zeroed memory
 */
void* kzalloc(size_t size) {
    void* ptr = kmalloc(size);
    if (ptr) {
        uint8_t* bytes = (uint8_t*)ptr;
        for (size_t i = 0; i < size; i++) {
            bytes[i] = 0;
        }
    }
    return ptr;
}

/**
 * Free memory
 */
void kfree(void* ptr) {
    if (!ptr) {
        return;
    }
    
    // Try to free from slab allocator first
    // We need to check if this pointer is in a slab page
    // For now, we'll check the heap magic first, and if it fails, try slab
    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);
    
    // Check if this looks like a heap block
    if (block->magic == HEAP_MAGIC) {
        // It's a heap block
        if (block->free) {
            kwarn("Heap: Double free detected: %p\n", ptr);
            return;
        }
        
        block->free = true;
        heap_used_size -= block->size + BLOCK_HEADER_SIZE;
        
        // Coalesce
        coalesce_free_blocks();
        return;
    }
    
    // Not a heap block, try slab allocator
    // Check if pointer is in a valid address range for slab pages
    // Slab pages are allocated from heap, so they're in heap address space
    // We'll let slab_free handle validation
    extern void slab_free(void* ptr, size_t size);
    
    // Try to determine size by checking which slab cache this might belong to
    // This is a heuristic - in production, we'd store size metadata
    // For now, we'll search through slab caches
    extern bool slab_try_free(void* ptr);
    if (slab_try_free(ptr)) {
        return;  // Successfully freed from slab
    }
    
    kerror("Heap: Invalid free (unknown allocator): %p\n", ptr);
}

/**
 * Reallocate memory
 */
void* krealloc(void* ptr, size_t new_size) {
    if (!ptr) {
        return kmalloc(new_size);
    }
    
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);
    
    if (block->size >= new_size) {
        // Current block is large enough
        return ptr;
    }
    
    // Allocate new block
    void* new_ptr = kmalloc(new_size);
    if (!new_ptr) {
        return NULL;
    }
    
    // Copy data
    uint8_t* src = (uint8_t*)ptr;
    uint8_t* dst = (uint8_t*)new_ptr;
    for (size_t i = 0; i < block->size; i++) {
        dst[i] = src[i];
    }
    
    // Free old block
    kfree(ptr);
    
    return new_ptr;
}

/**
 * Get heap statistics
 */
void heap_get_stats(size_t* total_size, size_t* used_size, size_t* free_size) {
    if (total_size) *total_size = heap_total_size;
    if (used_size) *used_size = heap_used_size;
    if (free_size) *free_size = heap_total_size - heap_used_size;
}

