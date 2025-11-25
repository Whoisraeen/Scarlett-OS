/**
 * @file malloc.c
 * @brief Memory allocation functions
 */

#include <stddef.h>
#include <stdint.h>
#include "syscall.h"

// Best-fit heap allocator with coalescing

typedef struct block_header {
    size_t size;           // Size of the data part
    struct block_header* next; // Next free block
    int free;              // Is this block free?
    uint32_t magic;        // Magic number for corruption detection
} block_header_t;

#define BLOCK_MAGIC 0xDEADBEEF
#define BLOCK_SIZE sizeof(block_header_t)

static block_header_t* free_list = NULL;
static void* heap_start = NULL;
static void* heap_end = NULL;

/**
 * Initialize heap
 */
static void init_heap(void) {
    if (heap_start == NULL) {
        // Get current brk
        heap_start = (void*)syscall(SYS_BRK, 0, 0, 0, 0, 0);
        heap_end = heap_start;
    }
}

/**
 * Request more memory from OS
 */
static block_header_t* request_space(block_header_t* last, size_t size) {
    block_header_t* block;
    void* request;
    
    request = (void*)syscall(SYS_BRK, (uint64_t)((char*)heap_end + size + BLOCK_SIZE), 0, 0, 0, 0);
    if (request == (void*)-1 || request == NULL) {
        return NULL; // sbrk failed
    }
    
    // If sbrk returns old break, we are good.
    // Wait, SYS_BRK returns current break. If we passed new break, it returns new break on success.
    // Let's assume syscall(SYS_BRK, new) returns new break.
    
    block = (block_header_t*)heap_end;
    block->size = size;
    block->next = NULL;
    block->free = 0;
    block->magic = BLOCK_MAGIC;
    
    heap_end = (void*)((char*)heap_end + size + BLOCK_SIZE);
    
    return block;
}

/**
 * Find a free block
 */
static block_header_t* find_free_block(block_header_t** last, size_t size) {
    block_header_t* current = free_list;
    block_header_t* best = NULL;
    
    while (current) {
        if (current->free && current->size >= size) {
            if (!best || current->size < best->size) {
                best = current;
            }
        }
        *last = current;
        current = current->next;
    }
    return best;
}

/**
 * Allocate memory
 */
void* malloc(size_t size) {
    if (size == 0) return NULL;
    
    init_heap();
    
    // Align to 8 bytes
    size = (size + 7) & ~7;
    
    block_header_t* block;
    
    if (!free_list) {
        // First allocation
        block = request_space(NULL, size);
        if (!block) return NULL;
        
        // We don't add to free list yet as it's allocated
        // But we need to track it? 
        // Actually, for a simple allocator, we usually traverse the whole heap.
        // But here we use an explicit free list.
        // If we use explicit free list, we only track FREE blocks.
        // So allocated blocks are not in the list?
        // Wait, if we only track free blocks, we can't coalesce easily without boundary tags or linear scan.
        // Let's use a linear list of ALL blocks for simplicity in this "proper" implementation.
        // Re-design: Linked list of ALL blocks.
        
        // Reset and start over with all-block list approach
        free_list = block; // free_list will point to the first block, free or not
    } else {
        block_header_t* last = free_list;
        block = find_free_block(&last, size); // Re-purposed to find best fit in all blocks
        
        if (block) {
            // Found a free block
            // Split if too large (threshold: 8 bytes payload)
            if (block->size >= size + BLOCK_SIZE + 8) {
                // Split
                block_header_t* new_block = (block_header_t*)((char*)block + BLOCK_SIZE + size);
                new_block->size = block->size - size - BLOCK_SIZE;
                new_block->next = block->next;
                new_block->free = 1;
                new_block->magic = BLOCK_MAGIC;
                
                block->size = size;
                block->next = new_block;
            }
            block->free = 0;
        } else {
            // No free block found, request more space
            block = request_space(last, size);
            if (!block) return NULL;
            last->next = block;
        }
    }
    
    return (void*)(block + 1);
}

/**
 * Free memory
 */
void free(void* ptr) {
    if (!ptr) return;
    
    block_header_t* block = (block_header_t*)ptr - 1;
    if (block->magic != BLOCK_MAGIC) {
        // Corruption or invalid pointer
        return;
    }
    
    block->free = 1;
    
    // Coalesce
    // Since we have a forward list, we can coalesce with next easily.
    // To coalesce with prev, we'd need a prev pointer or start from head.
    // For simplicity, just coalesce forward.
    if (block->next && block->next->free) {
        block->size += BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
    }
    
    // We should also coalesce with previous if possible, but that requires traversal
    // or a doubly linked list. Let's traverse from head for now (slow but correct).
    block_header_t* curr = free_list;
    while (curr && curr->next) {
        if (curr->free && curr->next->free && (char*)curr + BLOCK_SIZE + curr->size == (char*)curr->next) {
             curr->size += BLOCK_SIZE + curr->next->size;
             curr->next = curr->next->next;
        } else {
             curr = curr->next;
        }
    }
}

/**
 * Reallocate memory
 */
void* realloc(void* ptr, size_t size) {
    if (ptr == NULL) {
        return malloc(size);
    }
    
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    
    block_header_t* block = (block_header_t*)ptr - 1;
    if (block->size >= size) {
        // We have enough space, maybe split?
        // For now, just return same pointer
        return ptr;
    }
    
    // Need new block
    void* new_ptr = malloc(size);
    if (new_ptr) {
        // Copy data
        uint8_t* src = (uint8_t*)ptr;
        uint8_t* dst = (uint8_t*)new_ptr;
        for (size_t i = 0; i < block->size; i++) {
            dst[i] = src[i];
        }
        free(ptr);
    }
    return new_ptr;
}

/**
 * Allocate and zero memory
 */
void* calloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = malloc(total);
    if (ptr) {
        // Zero memory
        uint8_t* p = (uint8_t*)ptr;
        for (size_t i = 0; i < total; i++) {
            p[i] = 0;
        }
    }
    return ptr;
}

