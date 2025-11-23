/**
 * @file dma.c
 * @brief DMA (Direct Memory Access) infrastructure implementation
 * @details Advanced implementation with IOMMU page table management and IOVA allocation
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

// IOVA (I/O Virtual Address) Configuration
#define IOVA_START 0x10000000ULL      // Start at 256MB
#define IOVA_SIZE  0x40000000ULL      // 1GB IOVA space
#define IOVA_PAGE_SIZE 4096
#define IOVA_PAGES (IOVA_SIZE / IOVA_PAGE_SIZE)
#define IOVA_BITMAP_SIZE (IOVA_PAGES / 8)

// Intel VT-d / AMD-Vi Page Table Flags
#define IOMMU_PRESENT (1 << 0)
#define IOMMU_WRITE   (1 << 1)
#define IOMMU_READ    (1 << 0) // Often implied or different bit, using generalized model

// DMA buffer list
static dma_buffer_t* dma_buffers = NULL;
static spinlock_t dma_list_lock = SPINLOCK_INIT;
static uint64_t next_buffer_id = 1;

// IOMMU Context
typedef struct {
    paddr_t root_table;      // Physical address of PML4 root
    uint8_t* iova_bitmap;    // Bitmap for tracking IOVA allocation
    spinlock_t lock;         // Lock for IOMMU operations
    bool enabled;            // Is IOMMU enabled
} iommu_context_t;

static iommu_context_t iommu_ctx = {0};

// Helper: Bitmap operations
static void bitmap_set(uint8_t* map, size_t bit) {
    map[bit / 8] |= (1 << (bit % 8));
}

static void bitmap_clear(uint8_t* map, size_t bit) {
    map[bit / 8] &= ~(1 << (bit % 8));
}

static bool bitmap_test(uint8_t* map, size_t bit) {
    return (map[bit / 8] & (1 << (bit % 8))) != 0;
}

// Helper: Allocate contiguous IOVA range
static uint64_t alloc_iova_range(size_t pages) {
    spinlock_lock(&iommu_ctx.lock);
    
    size_t consecutive = 0;
    size_t start_idx = 0;
    
    for (size_t i = 0; i < IOVA_PAGES; i++) {
        if (!bitmap_test(iommu_ctx.iova_bitmap, i)) {
            if (consecutive == 0) start_idx = i;
            consecutive++;
            if (consecutive == pages) {
                // Found range, mark used
                for (size_t j = 0; j < pages; j++) {
                    bitmap_set(iommu_ctx.iova_bitmap, start_idx + j);
                }
                spinlock_unlock(&iommu_ctx.lock);
                return IOVA_START + (start_idx * IOVA_PAGE_SIZE);
            }
        } else {
            consecutive = 0;
        }
    }
    
    spinlock_unlock(&iommu_ctx.lock);
    return 0; // Out of IOVA space
}

// Helper: Free IOVA range
static void free_iova_range(uint64_t iova, size_t pages) {
    if (iova < IOVA_START) return;
    
    size_t start_idx = (iova - IOVA_START) / IOVA_PAGE_SIZE;
    if (start_idx >= IOVA_PAGES) return;
    
    spinlock_lock(&iommu_ctx.lock);
    for (size_t i = 0; i < pages; i++) {
        if (start_idx + i < IOVA_PAGES) {
            bitmap_clear(iommu_ctx.iova_bitmap, start_idx + i);
        }
    }
    spinlock_unlock(&iommu_ctx.lock);
}

/**
 * Flush cache for a memory region
 */
static void cache_flush(void* addr, size_t size) {
    uint8_t* p = (uint8_t*)addr;
    uint8_t* end = p + size;
    p = (uint8_t*)((uint64_t)p & ~63ULL);
    while (p < end) {
        __asm__ volatile("clflush (%0)" :: "r"(p) : "memory");
        p += 64;
    }
    __asm__ volatile("mfence" ::: "memory");
}

static void cache_invalidate(void* addr, size_t size) {
    cache_flush(addr, size);
}

// IOMMU Page Table Management
// 4-Level Page Table Structure (compatible with x86-64 / Intel VT-d)

static void* get_virtual_page(paddr_t phys) {
    return (void*)(phys + PHYS_MAP_BASE);
}

static void iommu_map_page(uint64_t iova, paddr_t phys, uint32_t flags) {
    spinlock_lock(&iommu_ctx.lock);
    
    uint64_t pml4_idx = (iova >> 39) & 0x1FF;
    uint64_t pdpt_idx = (iova >> 30) & 0x1FF;
    uint64_t pd_idx   = (iova >> 21) & 0x1FF;
    uint64_t pt_idx   = (iova >> 12) & 0x1FF;
    
    uint64_t* pml4 = (uint64_t*)get_virtual_page(iommu_ctx.root_table);
    
    // PML4 -> PDPT
    if (!(pml4[pml4_idx] & IOMMU_PRESENT)) {
        paddr_t pdpt_phys = pmm_alloc_page();
        if (!pdpt_phys) { spinlock_unlock(&iommu_ctx.lock); return; }
        memset(get_virtual_page(pdpt_phys), 0, PAGE_SIZE);
        pml4[pml4_idx] = pdpt_phys | IOMMU_PRESENT | IOMMU_WRITE | IOMMU_READ;
    }
    uint64_t* pdpt = (uint64_t*)get_virtual_page(pml4[pml4_idx] & ~0xFFF);
    
    // PDPT -> PD
    if (!(pdpt[pdpt_idx] & IOMMU_PRESENT)) {
        paddr_t pd_phys = pmm_alloc_page();
        if (!pd_phys) { spinlock_unlock(&iommu_ctx.lock); return; }
        memset(get_virtual_page(pd_phys), 0, PAGE_SIZE);
        pdpt[pdpt_idx] = pd_phys | IOMMU_PRESENT | IOMMU_WRITE | IOMMU_READ;
    }
    uint64_t* pd = (uint64_t*)get_virtual_page(pdpt[pdpt_idx] & ~0xFFF);
    
    // PD -> PT
    if (!(pd[pd_idx] & IOMMU_PRESENT)) {
        paddr_t pt_phys = pmm_alloc_page();
        if (!pt_phys) { spinlock_unlock(&iommu_ctx.lock); return; }
        memset(get_virtual_page(pt_phys), 0, PAGE_SIZE);
        pd[pd_idx] = pt_phys | IOMMU_PRESENT | IOMMU_WRITE | IOMMU_READ;
    }
    uint64_t* pt = (uint64_t*)get_virtual_page(pd[pd_idx] & ~0xFFF);
    
    // PT -> Phys
    pt[pt_idx] = phys | IOMMU_PRESENT | IOMMU_READ | IOMMU_WRITE; // TODO: Respect specific flags
    
    spinlock_unlock(&iommu_ctx.lock);
}

static void iommu_unmap_page(uint64_t iova) {
    spinlock_lock(&iommu_ctx.lock);
    
    uint64_t pml4_idx = (iova >> 39) & 0x1FF;
    uint64_t pdpt_idx = (iova >> 30) & 0x1FF;
    uint64_t pd_idx   = (iova >> 21) & 0x1FF;
    uint64_t pt_idx   = (iova >> 12) & 0x1FF;
    
    uint64_t* pml4 = (uint64_t*)get_virtual_page(iommu_ctx.root_table);
    
    if (!(pml4[pml4_idx] & IOMMU_PRESENT)) { spinlock_unlock(&iommu_ctx.lock); return; }
    uint64_t* pdpt = (uint64_t*)get_virtual_page(pml4[pml4_idx] & ~0xFFF);
    
    if (!(pdpt[pdpt_idx] & IOMMU_PRESENT)) { spinlock_unlock(&iommu_ctx.lock); return; }
    uint64_t* pd = (uint64_t*)get_virtual_page(pdpt[pdpt_idx] & ~0xFFF);
    
    if (!(pd[pd_idx] & IOMMU_PRESENT)) { spinlock_unlock(&iommu_ctx.lock); return; }
    uint64_t* pt = (uint64_t*)get_virtual_page(pd[pd_idx] & ~0xFFF);
    
    pt[pt_idx] = 0; // Clear entry
    
    // Invalidate TLB/IoTlb would go here
    
    spinlock_unlock(&iommu_ctx.lock);
}

/**
 * Initialize DMA subsystem
 */
void dma_init(void) {
    kinfo("Initializing DMA subsystem...\n");
    
    spinlock_init(&dma_list_lock);
    dma_buffers = NULL;
    next_buffer_id = 1;
    
    // Initialize IOMMU Context
    spinlock_init(&iommu_ctx.lock);
    
    // Check for IOMMU via ACPI or CPUID
    // Note: Real hardware detection would be more complex
    bool hardware_iommu_detected = false;
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000001));
    if (ecx & (1 << 2)) { // SVM/AMD-Vi
        hardware_iommu_detected = true;
    }
    // Note: VT-d detection usually requires parsing DMAR table which we assume we might find
    
    if (1) { // Force enable our software IOMMU manager regardless of hardware presence for now
             // In a real OS, this manages the page tables that the hardware *would* use.
        iommu_ctx.root_table = pmm_alloc_page();
        if (iommu_ctx.root_table) {
            memset(get_virtual_page(iommu_ctx.root_table), 0, PAGE_SIZE);
            
            // Allocate IOVA bitmap
            iommu_ctx.iova_bitmap = (uint8_t*)kmalloc(IOVA_BITMAP_SIZE);
            if (iommu_ctx.iova_bitmap) {
                memset(iommu_ctx.iova_bitmap, 0, IOVA_BITMAP_SIZE);
                iommu_ctx.enabled = true;
                kinfo("DMA: IOMMU subsystem initialized (Page Tables Active)\n");
            } else {
                kerror("DMA: Failed to allocate IOVA bitmap\n");
            }
        } else {
            kerror("DMA: Failed to allocate IOMMU root table\n");
        }
    }
}

static dma_buffer_t* find_dma_buffer(vaddr_t vaddr) {
    spinlock_lock(&dma_list_lock);
    for (dma_buffer_t* buf = dma_buffers; buf != NULL; buf = buf->next) {
        if (vaddr >= buf->virtual_address && vaddr < buf->virtual_address + buf->size) {
            spinlock_unlock(&dma_list_lock);
            return buf;
        }
    }
    spinlock_unlock(&dma_list_lock);
    return NULL;
}

void* dma_alloc(size_t size, uint32_t flags) {
    if (size == 0) return NULL;
    
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t actual_size = pages * PAGE_SIZE;
    
    paddr_t physical_addr = pmm_alloc_pages(pages);
    if (physical_addr == 0) {
        kerror("DMA: Failed to allocate physical pages\n");
        return NULL;
    }
    
    process_t* proc = process_get_current();
    address_space_t* as = proc ? process_get_address_space(proc) : NULL;
    if (!as) {
        pmm_free_pages(physical_addr, pages);
        return NULL;
    }
    
    // Linear search for free kernel virtual address (Simplified VMM part kept, focusing on IOMMU)
    // Ideally VMM has a vmm_alloc_region
    vaddr_t virtual_addr = DMA_BASE_VADDR + (next_buffer_id * 16 * 1024 * 1024); 
    // Check collision
    if (vmm_get_physical(as, virtual_addr) != 0) {
        virtual_addr += actual_size; // Try bumping
    }
    
    uint64_t vmm_flags = VMM_PRESENT | VMM_WRITE | VMM_USER | VMM_NX;
    if (flags & DMA_FLAG_UNCACHED) vmm_flags |= VMM_NOCACHE;
    if (flags & DMA_FLAG_WRITE_COMBINE) vmm_flags |= VMM_WRITETHROUGH;
    
    if (vmm_map_pages(as, virtual_addr, physical_addr, pages, vmm_flags) != 0) {
        pmm_free_pages(physical_addr, pages);
        return NULL;
    }
    
    dma_buffer_t* buffer = (dma_buffer_t*)kzalloc(sizeof(dma_buffer_t));
    if (!buffer) {
        vmm_unmap_pages(as, virtual_addr, pages);
        pmm_free_pages(physical_addr, pages);
        return NULL;
    }
    
    buffer->buffer_id = next_buffer_id++;
    buffer->physical_address = physical_addr;
    buffer->virtual_address = virtual_addr;
    buffer->size = actual_size;
    buffer->flags = flags;
    buffer->owner_tid = thread_current() ? thread_current()->tid : 0;
    buffer->device_id = 0;
    buffer->next = NULL;
    
    spinlock_lock(&dma_list_lock);
    buffer->next = dma_buffers;
    dma_buffers = buffer;
    spinlock_unlock(&dma_list_lock);
    
    // Zero buffer
    void* phys_virt = get_virtual_page(physical_addr);
    memset(phys_virt, 0, actual_size);
    
    kinfo("DMA: Alloc buf %lu (P:0x%lx, V:0x%lx, S:%lu)\n", 
          buffer->buffer_id, physical_addr, virtual_addr, actual_size);
    
    return (void*)virtual_addr;
}

int dma_free(void* vaddr) {
    if (!vaddr) return -1;
    vaddr_t vaddr_val = (vaddr_t)vaddr;
    
    dma_buffer_t* buffer = find_dma_buffer(vaddr_val);
    if (!buffer) return -1;
    
    if (buffer->device_id != 0) {
        kerror("DMA: Cannot free buffer still mapped to device %lu\n", buffer->device_id);
        return -1;
    }
    
    size_t pages = buffer->size / PAGE_SIZE;
    process_t* proc = process_get_current();
    if (proc) {
        address_space_t* as = process_get_address_space(proc);
        if (as) vmm_unmap_pages(as, buffer->virtual_address, pages);
    }
    
    pmm_free_pages(buffer->physical_address, pages);
    
    spinlock_lock(&dma_list_lock);
    if (dma_buffers == buffer) {
        dma_buffers = buffer->next;
    } else {
        dma_buffer_t* curr = dma_buffers;
        while (curr && curr->next != buffer) curr = curr->next;
        if (curr) curr->next = buffer->next;
    }
    spinlock_unlock(&dma_list_lock);
    
    kfree(buffer);
    return 0;
}

paddr_t dma_get_physical(void* vaddr) {
    if (!vaddr) return 0;
    dma_buffer_t* buffer = find_dma_buffer((vaddr_t)vaddr);
    if (!buffer) return 0;
    return buffer->physical_address + ((vaddr_t)vaddr - buffer->virtual_address);
}

int dma_sync(void* vaddr, size_t size, uint32_t direction) {
    if (!vaddr || size == 0) return -1;
    dma_buffer_t* buffer = find_dma_buffer((vaddr_t)vaddr);
    if (!buffer) return -1;
    
    if (buffer->flags & DMA_FLAG_COHERENT) return 0;
    
    vaddr_t start = (vaddr_t)vaddr;
    if (start < buffer->virtual_address) start = buffer->virtual_address;
    vaddr_t end = start + size;
    if (end > buffer->virtual_address + buffer->size) end = buffer->virtual_address + buffer->size;
    
    size_t actual_size = end - start;
    paddr_t phys_start = buffer->physical_address + (start - buffer->virtual_address);
    void* phys_ptr = get_virtual_page(phys_start);
    
    if (direction == DMA_DIR_TO_DEVICE || direction == DMA_DIR_BIDIRECTIONAL) {
        cache_flush(phys_ptr, actual_size);
    }
    // Invalidate is usually done for FROM_DEVICE to ensure CPU reads fresh data from DRAM
    if (direction == DMA_DIR_FROM_DEVICE || direction == DMA_DIR_BIDIRECTIONAL) {
        cache_invalidate(phys_ptr, actual_size);
    }
    
    return 0;
}

uint64_t dma_map_for_device(void* vaddr, uint64_t device_id) {
    if (!vaddr || device_id == 0) return 0;
    
    dma_buffer_t* buffer = find_dma_buffer((vaddr_t)vaddr);
    if (!buffer) return 0;
    
    // If no IOMMU, fallback to physical
    if (!iommu_ctx.enabled) {
        buffer->device_id = device_id;
        return buffer->physical_address;
    }
    
    // Allocate IOVA
    size_t pages = (buffer->size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t iova = alloc_iova_range(pages);
    
    if (iova == 0) {
        kerror("DMA: Failed to allocate IOVA space\n");
        return 0;
    }
    
    // Map pages in IOMMU
    for (size_t i = 0; i < pages; i++) {
        paddr_t page_phys = buffer->physical_address + (i * PAGE_SIZE);
        iommu_map_page(iova + (i * PAGE_SIZE), page_phys, 0);
    }
    
    buffer->device_id = device_id;
    buffer->iova = iova;
    kinfo("DMA: Mapped buf %lu to Dev %lu via IOMMU (IOVA: 0x%lx)\n", 
          buffer->buffer_id, device_id, iova);
          
    // In a real implementation, we would now update the device's Context Entry to point to iommu_ctx.root_table
    // if it wasn't already, or assume the system uses a global domain.
    
    return iova;
}

int dma_unmap_from_device(void* vaddr, uint64_t device_id) {
    if (!vaddr || device_id == 0) return -1;
    
    dma_buffer_t* buffer = find_dma_buffer((vaddr_t)vaddr);
    if (!buffer) return -1;
    
    if (buffer->device_id != device_id) return -1;
    
    if (iommu_ctx.enabled && buffer->iova != 0) {
        size_t pages = (buffer->size + PAGE_SIZE - 1) / PAGE_SIZE;
        
        // Unmap pages from IOMMU
        for (size_t i = 0; i < pages; i++) {
            iommu_unmap_page(buffer->iova + (i * PAGE_SIZE));
        }
        
        // Free IOVA range
        free_iova_range(buffer->iova, pages);
        buffer->iova = 0;
        
        kinfo("DMA: Unmapped buf %lu from Dev %lu\n", buffer->buffer_id, device_id);
    }
    
    buffer->device_id = 0;
    return 0;
}

bool dma_iommu_available(void) {
    return iommu_ctx.enabled;
}