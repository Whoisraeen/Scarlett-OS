/**
 * @file dma.h
 * @brief DMA (Direct Memory Access) infrastructure
 */

#ifndef KERNEL_MM_DMA_H
#define KERNEL_MM_DMA_H

#include "../types.h"
#include "../mm/vmm.h"

// DMA direction flags
#define DMA_DIR_TO_DEVICE   0  // CPU to device
#define DMA_DIR_FROM_DEVICE 1  // Device to CPU
#define DMA_DIR_BIDIRECTIONAL 2 // Both directions

// DMA buffer flags
#define DMA_FLAG_COHERENT     (1 << 0)  // Cache-coherent
#define DMA_FLAG_WRITE_COMBINE (1 << 1) // Write-combining
#define DMA_FLAG_UNCACHED     (1 << 2)  // Uncached
#define DMA_FLAG_IOMMU_PROTECT (1 << 3) // IOMMU protection enabled

// DMA buffer structure
typedef struct dma_buffer {
    uint64_t buffer_id;          // Unique buffer ID
    paddr_t physical_address;     // Physical address (for DMA)
    vaddr_t virtual_address;      // Virtual address (for CPU access)
    size_t size;                  // Size in bytes
    uint32_t flags;               // Buffer flags
    uint64_t owner_tid;           // Thread ID of owner
    uint64_t device_id;           // Device ID using this buffer (0 = none)
    uint64_t iova;                // I/O Virtual Address (if mapped via IOMMU)
    struct dma_buffer* next;
} dma_buffer_t;

/**
 * Initialize DMA subsystem
 */
void dma_init(void);

/**
 * Allocate a DMA buffer
 * @param size Size in bytes (will be rounded up to page size)
 * @param flags DMA buffer flags
 * @return Virtual address on success, NULL on error
 */
void* dma_alloc(size_t size, uint32_t flags);

/**
 * Free a DMA buffer
 * @param vaddr Virtual address returned by dma_alloc
 * @return 0 on success, -1 on error
 */
int dma_free(void* vaddr);

/**
 * Get physical address for a DMA buffer
 * @param vaddr Virtual address
 * @return Physical address or 0 on error
 */
paddr_t dma_get_physical(void* vaddr);

/**
 * Sync DMA buffer (flush/invalidate cache)
 * @param vaddr Virtual address
 * @param size Size in bytes
 * @param direction DMA direction
 * @return 0 on success, -1 on error
 */
int dma_sync(void* vaddr, size_t size, uint32_t direction);

/**
 * Map DMA buffer for device (IOMMU)
 * @param vaddr Virtual address
 * @param device_id Device ID
 * @return IOMMU address or 0 on error
 */
uint64_t dma_map_for_device(void* vaddr, uint64_t device_id);

/**
 * Unmap DMA buffer from device (IOMMU)
 * @param vaddr Virtual address
 * @param device_id Device ID
 * @return 0 on success, -1 on error
 */
int dma_unmap_from_device(void* vaddr, uint64_t device_id);

/**
 * Check if IOMMU is available
 * @return true if IOMMU is available, false otherwise
 */
bool dma_iommu_available(void);

#endif // KERNEL_MM_DMA_H

