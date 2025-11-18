/**
 * @file bootstrap.c
 * @brief Bootstrap allocator implementation
 *
 * Simple bump allocator for early boot, before heap is ready.
 * Used by VMM to allocate initial page tables.
 */

#include "../include/types.h"
#include "../include/config.h"
#include "../include/mm/bootstrap.h"
#include "../include/debug.h"

// Bootstrap heap configuration
#define BOOTSTRAP_HEAP_SIZE (256 * 1024)  // 256KB should be plenty for page tables

// Bootstrap heap (static allocation)
static uint8_t bootstrap_heap[BOOTSTRAP_HEAP_SIZE] __attribute__((aligned(16)));
static size_t bootstrap_offset = 0;
static bool bootstrap_active = true;

/**
 * Allocate memory from bootstrap heap
 */
void* bootstrap_alloc(size_t size) {
    if (!bootstrap_active) {
        kpanic("bootstrap_alloc() called after bootstrap disabled!");
    }

    if (size == 0) {
        return NULL;
    }

    // Align to 16 bytes
    size = ALIGN_UP(size, 16);

    // Check if we have enough space
    if (bootstrap_offset + size > BOOTSTRAP_HEAP_SIZE) {
        kerror("Bootstrap heap exhausted! Need %lu bytes, have %lu bytes free\n",
               size, BOOTSTRAP_HEAP_SIZE - bootstrap_offset);
        kpanic("Bootstrap heap exhausted");
    }

    // Allocate from heap
    void* ptr = &bootstrap_heap[bootstrap_offset];
    bootstrap_offset += size;

    kdebug("Bootstrap alloc: %lu bytes at %p (offset: %lu KB / %lu KB)\n",
           size, ptr, bootstrap_offset / 1024, BOOTSTRAP_HEAP_SIZE / 1024);

    return ptr;
}

/**
 * Zero-initialized bootstrap allocation
 */
void* bootstrap_zalloc(size_t size) {
    void* ptr = bootstrap_alloc(size);
    if (ptr) {
        // Zero the memory
        uint8_t* bytes = (uint8_t*)ptr;
        for (size_t i = 0; i < size; i++) {
            bytes[i] = 0;
        }
    }
    return ptr;
}

/**
 * Disable bootstrap allocator
 */
void bootstrap_disable(void) {
    if (!bootstrap_active) {
        kwarn("bootstrap_disable() called but bootstrap already disabled\n");
        return;
    }

    kinfo("Disabling bootstrap allocator (used %lu KB / %lu KB)\n",
          (unsigned long)(bootstrap_offset / 1024), (unsigned long)(BOOTSTRAP_HEAP_SIZE / 1024));

    bootstrap_active = false;
}

/**
 * Check if bootstrap allocator is active
 */
bool bootstrap_is_active(void) {
    return bootstrap_active;
}

/**
 * Get bootstrap allocator statistics
 */
void bootstrap_get_stats(size_t* used_out, size_t* free_out) {
    if (used_out) {
        *used_out = bootstrap_offset;
    }
    if (free_out) {
        *free_out = BOOTSTRAP_HEAP_SIZE - bootstrap_offset;
    }
}
