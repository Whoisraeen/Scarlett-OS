/**
 * @file malloc.c
 * @brief Memory allocation functions
 */

#include <stddef.h>
#include <stdint.h>
#include "syscall.h"

// Simple heap allocator using brk syscall
// This is a basic implementation - a real one would be more sophisticated

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
 * Allocate memory
 */
void* malloc(size_t size) {
    if (size == 0) return NULL;
    
    init_heap();
    
    // Align to 8 bytes
    size = (size + 7) & ~7;
    
    // Simple bump allocator
    void* ptr = heap_end;
    void* new_end = (char*)heap_end + size;
    
    // Extend heap
    if (syscall(SYS_BRK, (uint64_t)new_end, 0, 0, 0, 0) != 0) {
        return NULL;  // Out of memory
    }
    
    heap_end = new_end;
    return ptr;
}

/**
 * Free memory (no-op for bump allocator)
 */
void free(void* ptr) {
    (void)ptr;
    // Bump allocator doesn't support free
    // TODO: Implement proper heap allocator
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
    
    // TODO: Implement proper realloc
    void* new_ptr = malloc(size);
    if (new_ptr) {
        // Copy old data (would need to track size)
        // For now, just return new allocation
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

