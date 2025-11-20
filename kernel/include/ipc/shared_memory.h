/**
 * @file shared_memory.h
 * @brief Shared Memory IPC interface
 */

#ifndef KERNEL_IPC_SHARED_MEMORY_H
#define KERNEL_IPC_SHARED_MEMORY_H

#include "../types.h"
#include "../mm/vmm.h"

// Shared memory region structure
typedef struct shared_memory_region {
    uint64_t shm_id;                 // Shared memory ID
    paddr_t physical_base;            // Physical base address
    size_t size;                     // Size in bytes
    size_t refcount;                 // Reference count (number of processes using it)
    uint64_t creator_tid;            // Thread ID of creator
    uint32_t flags;                  // Flags (read-only, etc.)
    struct shared_memory_region* next;
} shared_memory_region_t;

// Shared memory mapping (per-process)
typedef struct shared_memory_mapping {
    uint64_t shm_id;                 // Shared memory ID
    address_space_t* address_space;  // Address space this is mapped into
    vaddr_t virtual_address;         // Virtual address in this address space
    size_t size;                     // Size of mapping
    struct shared_memory_mapping* next;
} shared_memory_mapping_t;

// Flags for shared memory creation
#define SHM_FLAG_READ_ONLY   (1 << 0)  // Read-only mapping
#define SHM_FLAG_EXECUTABLE  (1 << 1)  // Executable mapping

/**
 * Initialize shared memory system
 */
void shared_memory_init(void);

/**
 * Create a new shared memory region
 * @param size Size in bytes (will be rounded up to page size)
 * @param flags Creation flags
 * @return Shared memory ID or 0 on error
 */
uint64_t shared_memory_create(size_t size, uint32_t flags);

/**
 * Map a shared memory region into current process's address space
 * @param shm_id Shared memory ID
 * @param vaddr Virtual address to map at (0 for auto-allocate)
 * @param flags Mapping flags
 * @return Virtual address on success, 0 on error
 */
vaddr_t shared_memory_map(uint64_t shm_id, vaddr_t vaddr, uint32_t flags);

/**
 * Unmap a shared memory region from current process
 * @param shm_id Shared memory ID
 * @param vaddr Virtual address that was mapped
 * @return 0 on success, -1 on error
 */
int shared_memory_unmap(uint64_t shm_id, vaddr_t vaddr);

/**
 * Destroy a shared memory region (only if refcount is 0)
 * @param shm_id Shared memory ID
 * @return 0 on success, -1 on error
 */
int shared_memory_destroy(uint64_t shm_id);

/**
 * Get information about a shared memory region
 * @param shm_id Shared memory ID
 * @param size Output: size of region
 * @param refcount Output: reference count
 * @return 0 on success, -1 on error
 */
int shared_memory_get_info(uint64_t shm_id, size_t* size, size_t* refcount);

#endif // KERNEL_IPC_SHARED_MEMORY_H

