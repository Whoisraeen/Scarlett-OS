/**
 * @file shared_memory.c
 * @brief Shared Memory IPC implementation
 */

#include "../include/types.h"
#include "../include/ipc/shared_memory.h"
#include "../include/mm/vmm.h"
#include "../include/mm/pmm.h"
#include "../include/mm/heap.h"
#include "../include/sched/scheduler.h"
#include "../include/process.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/sync/spinlock.h"
#include "../include/config.h"
#include "../include/string.h"

#define MAX_SHM_REGIONS 256
#define SHM_BASE_VADDR 0x40000000ULL  // Base virtual address for shared memory (1GB)

// Shared memory region table
static shared_memory_region_t* shm_regions[MAX_SHM_REGIONS];
static uint64_t next_shm_id = 1;
static spinlock_t shm_table_lock = SPINLOCK_INIT;

// Per-process mapping list (one per address space)
static shared_memory_mapping_t* mapping_list = NULL;
static spinlock_t mapping_list_lock = SPINLOCK_INIT;

/**
 * Initialize shared memory system
 */
void shared_memory_init(void) {
    kinfo("Initializing shared memory system...\n");
    
    spinlock_init(&shm_table_lock);
    spinlock_init(&mapping_list_lock);
    
    for (int i = 0; i < MAX_SHM_REGIONS; i++) {
        shm_regions[i] = NULL;
    }
    
    mapping_list = NULL;
    
    kinfo("Shared memory system initialized\n");
}

/**
 * Find a shared memory region by ID
 */
static shared_memory_region_t* find_shm_region(uint64_t shm_id) {
    if (shm_id == 0 || shm_id >= MAX_SHM_REGIONS) {
        return NULL;
    }
    
    return shm_regions[shm_id];
}

/**
 * Find a mapping for a given address space and virtual address
 */
static shared_memory_mapping_t* find_mapping(address_space_t* as, vaddr_t vaddr) {
    spinlock_lock(&mapping_list_lock);
    
    for (shared_memory_mapping_t* m = mapping_list; m != NULL; m = m->next) {
        if (m->address_space == as && 
            vaddr >= m->virtual_address && 
            vaddr < m->virtual_address + m->size) {
            spinlock_unlock(&mapping_list_lock);
            return m;
        }
    }
    
    spinlock_unlock(&mapping_list_lock);
    return NULL;
}

/**
 * Create a new shared memory region
 */
uint64_t shared_memory_create(size_t size, uint32_t flags) {
    if (size == 0) {
        return 0;
    }
    
    // Round up to page size
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t actual_size = pages * PAGE_SIZE;
    
    spinlock_lock(&shm_table_lock);
    
    // Find free slot
    uint64_t shm_id = next_shm_id;
    if (shm_id >= MAX_SHM_REGIONS) {
        // Wrap around and find first free slot
        shm_id = 1;
        while (shm_id < MAX_SHM_REGIONS && shm_regions[shm_id] != NULL) {
            shm_id++;
        }
        if (shm_id >= MAX_SHM_REGIONS) {
            spinlock_unlock(&shm_table_lock);
            kerror("Shared memory: No free slots\n");
            return 0;
        }
    }
    
    // Allocate shared memory region structure
    shared_memory_region_t* region = (shared_memory_region_t*)kzalloc(sizeof(shared_memory_region_t));
    if (!region) {
        spinlock_unlock(&shm_table_lock);
        kerror("Shared memory: Out of memory for region structure\n");
        return 0;
    }
    
    // Allocate physical pages
    paddr_t physical_base = pmm_alloc_pages(pages);
    if (physical_base == 0) {
        kfree(region);
        spinlock_unlock(&shm_table_lock);
        kerror("Shared memory: Out of physical memory\n");
        return 0;
    }
    
    // Initialize region
    region->shm_id = shm_id;
    region->physical_base = physical_base;
    region->size = actual_size;
    region->refcount = 0;  // Will be incremented when mapped
    region->creator_tid = thread_current()->tid;
    region->flags = flags;
    region->next = NULL;
    
    // Insert into table
    shm_regions[shm_id] = region;
    
    if (shm_id == next_shm_id) {
        next_shm_id++;
    }
    
    spinlock_unlock(&shm_table_lock);
    
    kinfo("Shared memory: Created region %lu (size: %lu bytes, %lu pages)\n", 
          shm_id, actual_size, pages);
    
    return shm_id;
}

/**
 * Map a shared memory region into current process's address space
 */
vaddr_t shared_memory_map(uint64_t shm_id, vaddr_t vaddr, uint32_t flags) {
    if (shm_id == 0) {
        return 0;
    }
    
    spinlock_lock(&shm_table_lock);
    shared_memory_region_t* region = find_shm_region(shm_id);
    if (!region) {
        spinlock_unlock(&shm_table_lock);
        kerror("Shared memory: Region %lu not found\n", shm_id);
        return 0;
    }
    
    // Get current process's address space
    extern process_t* process_get_current(void);
    extern address_space_t* process_get_address_space(process_t* proc);
    process_t* proc = process_get_current();
    if (!proc) {
        spinlock_unlock(&shm_table_lock);
        kerror("Shared memory: No current process\n");
        return 0;
    }
    
    address_space_t* as = process_get_address_space(proc);
    if (!as) {
        spinlock_unlock(&shm_table_lock);
        kerror("Shared memory: No address space\n");
        return 0;
    }
    
    // If vaddr is 0, auto-allocate
    if (vaddr == 0) {
        // Find a free virtual address range
        // Simple approach: use SHM_BASE_VADDR + shm_id * 16MB
        vaddr = SHM_BASE_VADDR + (shm_id * 16 * 1024 * 1024);
        
        // Check if this address is already mapped
        extern paddr_t vmm_get_physical(address_space_t* as, vaddr_t vaddr);
        if (vmm_get_physical(as, vaddr) != 0) {
            // Try next page-aligned address
            vaddr = SHM_BASE_VADDR + (shm_id * 16 * 1024 * 1024) + (region->size);
        }
    }
    
    // Map physical pages into virtual address space
    size_t pages = region->size / PAGE_SIZE;
    uint64_t vmm_flags = VMM_PRESENT | VMM_USER;
    
    if (!(flags & SHM_FLAG_READ_ONLY)) {
        vmm_flags |= VMM_WRITE;
    }
    
    if (flags & SHM_FLAG_EXECUTABLE) {
        // Executable (no NX bit)
    } else {
        vmm_flags |= VMM_NX;
    }
    
    if (vmm_map_pages(as, vaddr, region->physical_base, pages, vmm_flags) != 0) {
        spinlock_unlock(&shm_table_lock);
        kerror("Shared memory: Failed to map pages\n");
        return 0;
    }
    
    // Increment reference count
    region->refcount++;
    
    spinlock_unlock(&shm_table_lock);
    
    // Add mapping to list
    shared_memory_mapping_t* mapping = (shared_memory_mapping_t*)kzalloc(sizeof(shared_memory_mapping_t));
    if (mapping) {
        mapping->shm_id = shm_id;
        mapping->address_space = as;
        mapping->virtual_address = vaddr;
        mapping->size = region->size;
        mapping->next = NULL;
        
        spinlock_lock(&mapping_list_lock);
        mapping->next = mapping_list;
        mapping_list = mapping;
        spinlock_unlock(&mapping_list_lock);
    }
    
    kinfo("Shared memory: Mapped region %lu at 0x%016lx (refcount: %lu)\n", 
          shm_id, vaddr, region->refcount);
    
    return vaddr;
}

/**
 * Unmap a shared memory region from current process
 */
int shared_memory_unmap(uint64_t shm_id, vaddr_t vaddr) {
    if (shm_id == 0) {
        return -1;
    }
    
    // Get current process's address space
    extern process_t* process_get_current(void);
    extern address_space_t* process_get_address_space(process_t* proc);
    process_t* proc = process_get_current();
    if (!proc) {
        return -1;
    }
    
    address_space_t* as = process_get_address_space(proc);
    if (!as) {
        return -1;
    }
    
    // Find mapping
    shared_memory_mapping_t* mapping = find_mapping(as, vaddr);
    if (!mapping || mapping->shm_id != shm_id) {
        kerror("Shared memory: Mapping not found\n");
        return -1;
    }
    
    // Unmap pages
    size_t pages = mapping->size / PAGE_SIZE;
    if (vmm_unmap_pages(as, vaddr, pages) != 0) {
        kerror("Shared memory: Failed to unmap pages\n");
        return -1;
    }
    
    // Remove mapping from list
    spinlock_lock(&mapping_list_lock);
    if (mapping_list == mapping) {
        mapping_list = mapping->next;
    } else {
        shared_memory_mapping_t* prev = mapping_list;
        while (prev && prev->next != mapping) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = mapping->next;
        }
    }
    spinlock_unlock(&mapping_list_lock);
    
    kfree(mapping);
    
    // Decrement reference count
    spinlock_lock(&shm_table_lock);
    shared_memory_region_t* region = find_shm_region(shm_id);
    if (region && region->refcount > 0) {
        region->refcount--;
    }
    spinlock_unlock(&shm_table_lock);
    
    kinfo("Shared memory: Unmapped region %lu from 0x%016lx\n", shm_id, vaddr);
    
    return 0;
}

/**
 * Destroy a shared memory region
 */
int shared_memory_destroy(uint64_t shm_id) {
    if (shm_id == 0) {
        return -1;
    }
    
    spinlock_lock(&shm_table_lock);
    shared_memory_region_t* region = find_shm_region(shm_id);
    if (!region) {
        spinlock_unlock(&shm_table_lock);
        return -1;
    }
    
    // Check if still in use
    if (region->refcount > 0) {
        spinlock_unlock(&shm_table_lock);
        kerror("Shared memory: Cannot destroy region %lu (still in use, refcount: %lu)\n", 
               shm_id, region->refcount);
        return -1;
    }
    
    // Free physical pages
    size_t pages = region->size / PAGE_SIZE;
    pmm_free_pages(region->physical_base, pages);
    
    // Remove from table
    shm_regions[shm_id] = NULL;
    
    spinlock_unlock(&shm_table_lock);
    
    kfree(region);
    
    kinfo("Shared memory: Destroyed region %lu\n", shm_id);
    
    return 0;
}

/**
 * Get information about a shared memory region
 */
int shared_memory_get_info(uint64_t shm_id, size_t* size, size_t* refcount) {
    if (shm_id == 0 || !size || !refcount) {
        return -1;
    }
    
    spinlock_lock(&shm_table_lock);
    shared_memory_region_t* region = find_shm_region(shm_id);
    if (!region) {
        spinlock_unlock(&shm_table_lock);
        return -1;
    }
    
    *size = region->size;
    *refcount = region->refcount;
    
    spinlock_unlock(&shm_table_lock);
    
    return 0;
}

