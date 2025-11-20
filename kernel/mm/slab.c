/**
 * @file slab.c
 * @brief Slab allocator implementation for kernel objects
 */

#include "../include/types.h"
#include "../include/mm/slab.h"
#include "../include/mm/heap.h"
#include "../include/mm/pmm.h"
#include "../include/mm/vmm.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/sync/spinlock.h"
#include "../include/config.h"
#include "../include/string.h"

// Slab sizes (must be power of 2 and >= 8)
static const size_t slab_sizes[NUM_SLAB_SIZES] = {
    8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 0
};

#define SLAB_PAGE_SIZE PAGE_SIZE
#define SLAB_OBJECTS_PER_PAGE(size) ((SLAB_PAGE_SIZE - sizeof(slab_page_t)) / size)

// Slab page header
typedef struct slab_page {
    struct slab_page* next;           // Next page in this slab cache
    struct slab_page* prev;           // Previous page
    uint32_t free_count;              // Number of free objects
    uint32_t total_objects;           // Total objects in this page
    uint32_t size_class;              // Size class index
    uint8_t bitmap[0];                // Bitmap for free objects (variable size)
} slab_page_t;

// Slab cache (one per size class)
typedef struct slab_cache {
    size_t object_size;               // Size of objects in this cache
    slab_page_t* partial_pages;       // Pages with some free objects
    slab_page_t* full_pages;          // Pages with no free objects
    uint32_t total_pages;             // Total pages in this cache
    uint32_t total_objects;           // Total objects allocated
    uint32_t free_objects;            // Free objects available
    spinlock_t lock;                  // Lock for this cache
} slab_cache_t;

// Slab caches (one per size class)
static slab_cache_t slab_caches[NUM_SLAB_SIZES];
static bool slab_initialized = false;

/**
 * Find size class index for a given size
 */
static uint32_t find_size_class(size_t size) {
    for (uint32_t i = 0; i < NUM_SLAB_SIZES; i++) {
        if (slab_sizes[i] >= size) {
            return i;
        }
    }
    return NUM_SLAB_SIZES - 1;  // Use largest size class
}

/**
 * Get bitmap size for a page
 */
static size_t get_bitmap_size(uint32_t object_count) {
    return (object_count + 7) / 8;  // Round up to bytes
}

/**
 * Initialize a slab cache
 */
static void init_slab_cache(slab_cache_t* cache, size_t object_size) {
    cache->object_size = object_size;
    cache->partial_pages = NULL;
    cache->full_pages = NULL;
    cache->total_pages = 0;
    cache->total_objects = 0;
    cache->free_objects = 0;
    spinlock_init(&cache->lock);
}

/**
 * Allocate a new slab page
 */
static slab_page_t* alloc_slab_page(uint32_t size_class) {
    size_t object_size = slab_sizes[size_class];
    uint32_t objects_per_page = SLAB_OBJECTS_PER_PAGE(object_size);
    
    // Allocate page from heap
    size_t page_size = SLAB_PAGE_SIZE;
    void* page_mem = kmalloc(page_size);
    if (!page_mem) {
        return NULL;
    }
    
    // Initialize page header
    slab_page_t* page = (slab_page_t*)page_mem;
    page->next = NULL;
    page->prev = NULL;
    page->free_count = objects_per_page;
    page->total_objects = objects_per_page;
    page->size_class = size_class;
    
    // Initialize bitmap (all objects free)
    size_t bitmap_size = get_bitmap_size(objects_per_page);
    uint8_t* bitmap = page->bitmap;
    for (size_t i = 0; i < bitmap_size; i++) {
        bitmap[i] = 0xFF;  // All bits set = all free
    }
    
    return page;
}

/**
 * Find free object in a page
 */
static void* find_free_object(slab_page_t* page) {
    size_t object_size = slab_sizes[page->size_class];
    uint32_t objects_per_page = page->total_objects;
    size_t bitmap_size = get_bitmap_size(objects_per_page);
    uint8_t* bitmap = page->bitmap;
    
    // Find first free bit
    for (uint32_t byte = 0; byte < bitmap_size; byte++) {
        if (bitmap[byte] != 0) {
            // Find first set bit in this byte
            for (uint32_t bit = 0; bit < 8; bit++) {
                if (bitmap[byte] & (1 << bit)) {
                    uint32_t object_index = byte * 8 + bit;
                    if (object_index >= objects_per_page) {
                        break;
                    }
                    
                    // Mark as used
                    bitmap[byte] &= ~(1 << bit);
                    page->free_count--;
                    
                    // Calculate object address
                    size_t header_size = sizeof(slab_page_t) + bitmap_size;
                    void* object = (uint8_t*)page + header_size + (object_index * object_size);
                    
                    return object;
                }
            }
        }
    }
    
    return NULL;  // No free objects
}

/**
 * Free an object in a page
 */
static void free_object_in_page(slab_page_t* page, void* ptr) {
    size_t object_size = slab_sizes[page->size_class];
    uint32_t objects_per_page = page->total_objects;
    size_t bitmap_size = get_bitmap_size(objects_per_page);
    
    // Calculate object index
    size_t header_size = sizeof(slab_page_t) + bitmap_size;
    uintptr_t object_offset = (uintptr_t)ptr - ((uintptr_t)page + header_size);
    uint32_t object_index = object_offset / object_size;
    
    if (object_index >= objects_per_page) {
        kerror("Slab: Invalid object index %u\n", object_index);
        return;
    }
    
    // Mark as free
    uint32_t byte = object_index / 8;
    uint32_t bit = object_index % 8;
    page->bitmap[byte] |= (1 << bit);
    page->free_count++;
}

/**
 * Initialize slab allocator
 */
void slab_init(void) {
    if (slab_initialized) {
        return;
    }
    
    kinfo("Initializing slab allocator...\n");
    
    for (uint32_t i = 0; i < NUM_SLAB_SIZES; i++) {
        init_slab_cache(&slab_caches[i], slab_sizes[i]);
    }
    
    slab_initialized = true;
    
    kinfo("Slab allocator initialized (%u size classes)\n", NUM_SLAB_SIZES);
}

/**
 * Allocate from slab
 */
void* slab_alloc(size_t size) {
    if (size == 0 || size > SLAB_SIZE_4096) {
        return NULL;  // Use regular heap for large allocations
    }
    
    uint32_t size_class = find_size_class(size);
    slab_cache_t* cache = &slab_caches[size_class];
    
    spinlock_lock(&cache->lock);
    
    // Try to find a page with free objects
    slab_page_t* page = cache->partial_pages;
    void* object = NULL;
    
    if (page) {
        object = find_free_object(page);
        
        if (object) {
            // If page is now full, move to full_pages list
            if (page->free_count == 0) {
                // Remove from partial_pages
                if (page->prev) {
                    page->prev->next = page->next;
                } else {
                    cache->partial_pages = page->next;
                }
                if (page->next) {
                    page->next->prev = page->prev;
                }
                
                // Add to full_pages
                page->next = cache->full_pages;
                page->prev = NULL;
                if (cache->full_pages) {
                    cache->full_pages->prev = page;
                }
                cache->full_pages = page;
            }
            
            cache->total_objects++;
            cache->free_objects--;
            spinlock_unlock(&cache->lock);
            return object;
        }
    }
    
    // No free objects, allocate new page
    page = alloc_slab_page(size_class);
    if (!page) {
        spinlock_unlock(&cache->lock);
        return NULL;
    }
    
    // Get first object from new page
    object = find_free_object(page);
    
    // Add to partial_pages
    page->next = cache->partial_pages;
    page->prev = NULL;
    if (cache->partial_pages) {
        cache->partial_pages->prev = page;
    }
    cache->partial_pages = page;
    
    cache->total_pages++;
    cache->total_objects++;
    cache->free_objects += page->free_count - 1;  // -1 for the one we just allocated
    
    spinlock_unlock(&cache->lock);
    
    return object;
}

/**
 * Find page containing a pointer (searches all caches)
 */
static slab_page_t* find_page_for_ptr(void* ptr) {
    for (uint32_t i = 0; i < NUM_SLAB_SIZES; i++) {
        slab_cache_t* cache = &slab_caches[i];
        
        // Search partial pages
        slab_page_t* page = cache->partial_pages;
        while (page) {
            if ((uintptr_t)ptr >= (uintptr_t)page && 
                (uintptr_t)ptr < (uintptr_t)page + SLAB_PAGE_SIZE) {
                return page;
            }
            page = page->next;
        }
        
        // Search full pages
        page = cache->full_pages;
        while (page) {
            if ((uintptr_t)ptr >= (uintptr_t)page && 
                (uintptr_t)ptr < (uintptr_t)page + SLAB_PAGE_SIZE) {
                return page;
            }
            page = page->next;
        }
    }
    
    return NULL;
}

/**
 * Free memory allocated by slab_alloc
 */
void slab_free(void* ptr, size_t size) {
    if (!ptr || size == 0 || size > SLAB_SIZE_4096) {
        return;
    }
    
    uint32_t size_class = find_size_class(size);
    slab_cache_t* cache = &slab_caches[size_class];
    
    spinlock_lock(&cache->lock);
    
    // Find which page this object belongs to
    slab_page_t* page = find_page_for_ptr(ptr);
    
    if (!page || page->size_class != size_class) {
        spinlock_unlock(&cache->lock);
        kerror("Slab: Object %p not found in cache for size %lu\n", ptr, size);
        return;
    }
    
    // If page is in full_pages, move to partial_pages
    if (page->free_count == 0) {
        // Remove from full_pages
        if (page->prev) {
            page->prev->next = page->next;
        } else {
            cache->full_pages = page->next;
        }
        if (page->next) {
            page->next->prev = page->prev;
        }
        
        // Add to partial_pages
        page->next = cache->partial_pages;
        page->prev = NULL;
        if (cache->partial_pages) {
            cache->partial_pages->prev = page;
        }
        cache->partial_pages = page;
    }
    
    // Free the object
    free_object_in_page(page, ptr);
    
    cache->total_objects--;
    cache->free_objects++;
    
    spinlock_unlock(&cache->lock);
}

/**
 * Try to free a pointer (auto-detect size class)
 */
bool slab_try_free(void* ptr) {
    if (!ptr) {
        return false;
    }
    
    // Find which page this object belongs to
    slab_page_t* page = find_page_for_ptr(ptr);
    if (!page) {
        return false;  // Not a slab allocation
    }
    
    uint32_t size_class = page->size_class;
    slab_cache_t* cache = &slab_caches[size_class];
    
    spinlock_lock(&cache->lock);
    
    // Verify page is still valid
    if (page->size_class != size_class) {
        spinlock_unlock(&cache->lock);
        return false;
    }
    
    // If page is in full_pages, move to partial_pages
    if (page->free_count == 0) {
        // Remove from full_pages
        if (page->prev) {
            page->prev->next = page->next;
        } else {
            cache->full_pages = page->next;
        }
        if (page->next) {
            page->next->prev = page->prev;
        }
        
        // Add to partial_pages
        page->next = cache->partial_pages;
        page->prev = NULL;
        if (cache->partial_pages) {
            cache->partial_pages->prev = page;
        }
        cache->partial_pages = page;
    }
    
    // Free the object
    free_object_in_page(page, ptr);
    
    cache->total_objects--;
    cache->free_objects++;
    
    spinlock_unlock(&cache->lock);
    
    return true;
}

/**
 * Get slab statistics
 */
void slab_get_stats(size_t* total_objects, size_t* free_objects, size_t* used_objects) {
    size_t total = 0, free = 0, used = 0;
    
    for (uint32_t i = 0; i < NUM_SLAB_SIZES; i++) {
        slab_cache_t* cache = &slab_caches[i];
        total += cache->total_objects;
        free += cache->free_objects;
        used += (cache->total_objects - cache->free_objects);
    }
    
    if (total_objects) *total_objects = total;
    if (free_objects) *free_objects = free;
    if (used_objects) *used_objects = used;
}

