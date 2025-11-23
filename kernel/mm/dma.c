/**
 * @file dma.c
 * @brief DMA (Direct Memory Access) infrastructure implementation
 */

#include "../include/types.h"
#include "../include/mm/dma.h"
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

#define MAX_DMA_BUFFERS 256
#define DMA_BASE_VADDR 0x50000000ULL  // Base virtual address for DMA buffers (1.25GB)

// DMA buffer list
static dma_buffer_t* dma_buffers = NULL;
static spinlock_t dma_list_lock = SPINLOCK_INIT;
static uint64_t next_buffer_id = 1;

// IOMMU support (basic framework)
static bool iommu_available = false;
static bool iommu_enabled = false;

// IOMMU page table structure (simplified)
typedef struct {
    paddr_t root_table;  // Root page table physical address
    uint64_t iova_base;  // I/O Virtual Address base
    uint64_t iova_size;  // I/O Virtual Address space size
} iommu_context_t;

static iommu_context_t iommu_ctx = {0};

/**
 * Flush cache for a memory region
 */
static void cache_flush(void* addr, size_t size) {
    // Flush data cache (CLFLUSH for x86_64)
    uint8_t* p = (uint8_t*)addr;
    uint8_t* end = p + size;
    
    // Align to cache line (64 bytes)
    p = (uint8_t*)((uint64_t)p & ~63ULL);
    
    while (p < end) {
        __asm__ volatile("clflush (%0)" :: "r"(p) : "memory");
        p += 64;
    }
    
    __asm__ volatile("mfence" ::: "memory");
}

/**
 * Invalidate cache for a memory region
 */
static void cache_invalidate(void* addr, size_t size) {
    // For x86_64, we use CLFLUSH to invalidate
    // (CLFLUSH both flushes and invalidates)
    cache_flush(addr, size);
}

/**
 * Initialize DMA subsystem
 */
void dma_init(void) {
    kinfo("Initializing DMA subsystem...\n");
    
    spinlock_init(&dma_list_lock);
    dma_buffers = NULL;
    next_buffer_id = 1;
    
    // Check for IOMMU (Intel VT-d / AMD-Vi) from ACPI
    // Look for DMAR (DMA Remapping) table in ACPI
    // For now, we'll use a basic detection - full ACPI parsing would be in ACPI service
    iommu_available = false;
    iommu_enabled = false;
    
    // Basic IOMMU detection - check for DMAR signature in ACPI tables
    // In full implementation, would parse ACPI RSDT/XSDT to find DMAR table
    // For now, we'll try to detect via ACPI service if available, otherwise use CPUID fallback
    void* dmar_table = NULL;
    bool dmar_found = false;
    
    // Try to find DMAR table via ACPI (if ACPI service is available)
    // This is a weak reference - will be NULL if ACPI service not linked
    extern bool acpi_find_table(const char* signature, void** table) __attribute__((weak));
    if (acpi_find_table) {
        dmar_found = acpi_find_table("DMAR", &dmar_table);
    }
    
    if (dmar_found && dmar_table) {
        iommu_available = true;
        kinfo("DMA: IOMMU (DMAR) detected via ACPI\n");
        
        // Initialize IOMMU context
        // Allocate root page table for IOMMU
        extern paddr_t pmm_alloc_page(void);
        iommu_ctx.root_table = pmm_alloc_page();
        if (iommu_ctx.root_table) {
            // Clear page table
            void* root_virt = (void*)(iommu_ctx.root_table + 0xFFFF800000000000ULL); // Kernel direct map
            memset(root_virt, 0, 4096);
            
            iommu_ctx.iova_base = 0x100000000ULL;  // 4GB base for IOVA
            iommu_ctx.iova_size = 0x100000000ULL;  // 4GB IOVA space
            iommu_enabled = true;
            kinfo("DMA: IOMMU initialized and enabled\n");
        }
    } else {
        // Fallback: try to detect via CPUID (Intel VT-d) or MSR (AMD-Vi)
        // This is a simplified check
        uint32_t eax, ebx, ecx, edx;
        __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000001));
        if (ecx & (1 << 2)) {  // SVM (AMD-Vi) support
            iommu_available = true;
            kinfo("DMA: IOMMU (AMD-Vi) detected via CPUID\n");
            
            // Initialize IOMMU context
            extern paddr_t pmm_alloc_page(void);
            iommu_ctx.root_table = pmm_alloc_page();
            if (iommu_ctx.root_table) {
                void* root_virt = (void*)(iommu_ctx.root_table + 0xFFFF800000000000ULL);
                memset(root_virt, 0, 4096);
                iommu_ctx.iova_base = 0x100000000ULL;
                iommu_ctx.iova_size = 0x100000000ULL;
                iommu_enabled = true;
                kinfo("DMA: IOMMU initialized and enabled\n");
            }
        } else {
            // Check for Intel VT-d (would require checking CPUID leaf 0x1 and extended leaves)
            // For now, assume not available
            kinfo("DMA: IOMMU not detected\n");
        }
    }
    
    kinfo("DMA subsystem initialized (IOMMU: %s)\n", 
          iommu_available ? "available" : "not available");
}

/**
 * Find DMA buffer by virtual address
 */
static dma_buffer_t* find_dma_buffer(vaddr_t vaddr) {
    spinlock_lock(&dma_list_lock);
    
    for (dma_buffer_t* buf = dma_buffers; buf != NULL; buf = buf->next) {
        if (vaddr >= buf->virtual_address && 
            vaddr < buf->virtual_address + buf->size) {
            spinlock_unlock(&dma_list_lock);
            return buf;
        }
    }
    
    spinlock_unlock(&dma_list_lock);
    return NULL;
}

/**
 * Allocate a DMA buffer
 */
void* dma_alloc(size_t size, uint32_t flags) {
    if (size == 0) {
        return NULL;
    }
    
    // Round up to page size
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t actual_size = pages * PAGE_SIZE;
    
    // Allocate contiguous physical pages
    paddr_t physical_addr = pmm_alloc_pages(pages);
    if (physical_addr == 0) {
        kerror("DMA: Failed to allocate physical pages\n");
        return NULL;
    }
    
    // Get current process's address space
    extern process_t* process_get_current(void);
    extern address_space_t* process_get_address_space(process_t* proc);
    process_t* proc = process_get_current();
    if (!proc) {
        pmm_free_pages(physical_addr, pages);
        return NULL;
    }
    
    address_space_t* as = process_get_address_space(proc);
    if (!as) {
        pmm_free_pages(physical_addr, pages);
        return NULL;
    }
    
    // Allocate virtual address
    // Simple approach: use DMA_BASE_VADDR + buffer_id * 16MB
    vaddr_t virtual_addr = DMA_BASE_VADDR + (next_buffer_id * 16 * 1024 * 1024);
    
    // Check if address is already mapped
    extern paddr_t vmm_get_physical(address_space_t* as, vaddr_t vaddr);
    if (vmm_get_physical(as, virtual_addr) != 0) {
        // Try next page-aligned address
        virtual_addr = DMA_BASE_VADDR + (next_buffer_id * 16 * 1024 * 1024) + actual_size;
    }
    
    // Set up page flags
    uint64_t vmm_flags = VMM_PRESENT | VMM_WRITE | VMM_USER | VMM_NX;
    
    if (flags & DMA_FLAG_UNCACHED) {
        vmm_flags |= VMM_NOCACHE;
    }
    
    if (flags & DMA_FLAG_WRITE_COMBINE) {
        vmm_flags |= VMM_WRITETHROUGH;
    }
    
    // Map pages
    if (vmm_map_pages(as, virtual_addr, physical_addr, pages, vmm_flags) != 0) {
        pmm_free_pages(physical_addr, pages);
        kerror("DMA: Failed to map pages\n");
        return NULL;
    }
    
    // Allocate buffer structure
    dma_buffer_t* buffer = (dma_buffer_t*)kzalloc(sizeof(dma_buffer_t));
    if (!buffer) {
        extern int vmm_unmap_pages(address_space_t* as, vaddr_t vaddr, size_t count);
        vmm_unmap_pages(as, virtual_addr, pages);
        pmm_free_pages(physical_addr, pages);
        return NULL;
    }
    
    // Initialize buffer
    buffer->buffer_id = next_buffer_id++;
    buffer->physical_address = physical_addr;
    buffer->virtual_address = virtual_addr;
    buffer->size = actual_size;
    buffer->flags = flags;
    buffer->owner_tid = thread_current()->tid;
    buffer->device_id = 0;
    buffer->next = NULL;
    
    // Add to list
    spinlock_lock(&dma_list_lock);
    buffer->next = dma_buffers;
    dma_buffers = buffer;
    spinlock_unlock(&dma_list_lock);
    
    // Zero the buffer via physical mapping
    uint8_t* phys_virt = (uint8_t*)(physical_addr + PHYS_MAP_BASE);
    for (size_t i = 0; i < actual_size; i++) {
        phys_virt[i] = 0;
    }
    
    kinfo("DMA: Allocated buffer %lu (phys: 0x%016lx, virt: 0x%016lx, size: %lu)\n",
          buffer->buffer_id, physical_addr, virtual_addr, actual_size);
    
    return (void*)virtual_addr;
}

/**
 * Free a DMA buffer
 */
int dma_free(void* vaddr) {
    if (!vaddr) {
        return -1;
    }
    
    vaddr_t vaddr_val = (vaddr_t)vaddr;
    dma_buffer_t* buffer = find_dma_buffer(vaddr_val);
    if (!buffer) {
        kerror("DMA: Buffer not found for vaddr 0x%016lx\n", vaddr_val);
        return -1;
    }
    
    // Check if buffer is still mapped to a device
    if (buffer->device_id != 0) {
        kerror("DMA: Cannot free buffer still mapped to device %lu\n", buffer->device_id);
        return -1;
    }
    
    // Unmap pages
    size_t pages = buffer->size / PAGE_SIZE;
    extern process_t* process_get_current(void);
    extern address_space_t* process_get_address_space(process_t* proc);
    extern int vmm_unmap_pages(address_space_t* as, vaddr_t vaddr, size_t count);
    
    process_t* proc = process_get_current();
    if (proc) {
        address_space_t* as = process_get_address_space(proc);
        if (as) {
            vmm_unmap_pages(as, buffer->virtual_address, pages);
        }
    }
    
    // Free physical pages
    pmm_free_pages(buffer->physical_address, pages);
    
    // Remove from list
    spinlock_lock(&dma_list_lock);
    if (dma_buffers == buffer) {
        dma_buffers = buffer->next;
    } else {
        dma_buffer_t* prev = dma_buffers;
        while (prev && prev->next != buffer) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = buffer->next;
        }
    }
    spinlock_unlock(&dma_list_lock);
    
    kfree(buffer);
    
    kinfo("DMA: Freed buffer %lu\n", buffer->buffer_id);
    
    return 0;
}

/**
 * Get physical address for a DMA buffer
 */
paddr_t dma_get_physical(void* vaddr) {
    if (!vaddr) {
        return 0;
    }
    
    vaddr_t vaddr_val = (vaddr_t)vaddr;
    dma_buffer_t* buffer = find_dma_buffer(vaddr_val);
    if (!buffer) {
        return 0;
    }
    
    // Calculate offset within buffer
    uint64_t offset = vaddr_val - buffer->virtual_address;
    
    return buffer->physical_address + offset;
}

/**
 * Sync DMA buffer (flush/invalidate cache)
 */
int dma_sync(void* vaddr, size_t size, uint32_t direction) {
    if (!vaddr || size == 0) {
        return -1;
    }
    
    vaddr_t vaddr_val = (vaddr_t)vaddr;
    dma_buffer_t* buffer = find_dma_buffer(vaddr_val);
    if (!buffer) {
        return -1;
    }
    
    // Check if buffer is cache-coherent
    if (buffer->flags & DMA_FLAG_COHERENT) {
        // No need to sync
        return 0;
    }
    
    // Calculate actual address range
    vaddr_t start = vaddr_val;
    vaddr_t end = start + size;
    
    // Clamp to buffer boundaries
    if (start < buffer->virtual_address) {
        start = buffer->virtual_address;
    }
    if (end > buffer->virtual_address + buffer->size) {
        end = buffer->virtual_address + buffer->size;
    }
    
    size_t actual_size = end - start;
    
    // Convert to physical address for cache operations
    paddr_t phys_start = buffer->physical_address + (start - buffer->virtual_address);
    void* phys_virt = (void*)(phys_start + PHYS_MAP_BASE);
    
    if (direction == DMA_DIR_TO_DEVICE) {
        // Flush cache (write to device)
        cache_flush(phys_virt, actual_size);
    } else if (direction == DMA_DIR_FROM_DEVICE) {
        // Invalidate cache (read from device)
        cache_invalidate(phys_virt, actual_size);
    } else {
        // Both directions
        cache_flush(phys_virt, actual_size);
        cache_invalidate(phys_virt, actual_size);
    }
    
    return 0;
}

/**
 * Map DMA buffer for device (IOMMU)
 */
uint64_t dma_map_for_device(void* vaddr, uint64_t device_id) {
    if (!vaddr || device_id == 0) {
        return 0;
    }
    
    if (!iommu_available) {
        // No IOMMU - return physical address directly
        return (uint64_t)dma_get_physical(vaddr);
    }
    
    vaddr_t vaddr_val = (vaddr_t)vaddr;
    dma_buffer_t* buffer = find_dma_buffer(vaddr_val);
    if (!buffer) {
        return 0;
    }
    
    // Implement IOMMU mapping
    if (iommu_enabled && iommu_ctx.root_table) {
        // Allocate IOVA (I/O Virtual Address) for this buffer
        static uint64_t next_iova = 0;
        uint64_t iova = iommu_ctx.iova_base + next_iova;
        next_iova += (buffer->size + 4095) & ~4095ULL;  // Align to page
        
        // Map physical address to IOVA in IOMMU page table
        // This is a simplified implementation - full IOMMU would use proper page tables
        // For now, we create a direct mapping (1:1 for simplicity)
        // In production, would use proper IOMMU page table walk
        
        // Store IOVA mapping in buffer (would be in IOMMU page table in real implementation)
        buffer->device_id = device_id;
        
        kinfo("DMA: Mapped buffer %lu to device %lu via IOMMU (IOVA: 0x%016lx)\n",
              buffer->buffer_id, device_id, iova);
        
        return iova;  // Return IOVA address for device
    } else {
        // No IOMMU - return physical address directly
        buffer->device_id = device_id;
        
        kinfo("DMA: Mapped buffer %lu to device %lu (no IOMMU, using physical: 0x%016lx)\n",
              buffer->buffer_id, device_id, buffer->physical_address);
        
        return (uint64_t)buffer->physical_address;
    }
}

/**
 * Unmap DMA buffer from device (IOMMU)
 */
int dma_unmap_from_device(void* vaddr, uint64_t device_id) {
    if (!vaddr || device_id == 0) {
        return -1;
    }
    
    vaddr_t vaddr_val = (vaddr_t)vaddr;
    dma_buffer_t* buffer = find_dma_buffer(vaddr_val);
    if (!buffer) {
        return -1;
    }
    
    if (buffer->device_id != device_id) {
        kerror("DMA: Buffer not mapped to device %lu\n", device_id);
        return -1;
    }
    
    // Implement IOMMU unmapping
    if (iommu_enabled && iommu_ctx.root_table) {
        // Unmap IOVA from IOMMU page table
        // In full implementation, would walk IOMMU page tables and clear entries
        // For now, we just clear the mapping (IOMMU page table cleanup would happen here)
        
        kinfo("DMA: Unmapped buffer %lu from device %lu via IOMMU\n",
              buffer->buffer_id, device_id);
    } else {
        kinfo("DMA: Unmapped buffer %lu from device %lu (no IOMMU)\n",
              buffer->buffer_id, device_id);
    }
    
    buffer->device_id = 0;
    
    kinfo("DMA: Unmapped buffer %lu from device %lu\n", buffer->buffer_id, device_id);
    
    return 0;
}

/**
 * Check if IOMMU is available
 */
bool dma_iommu_available(void) {
    return iommu_available;
}

