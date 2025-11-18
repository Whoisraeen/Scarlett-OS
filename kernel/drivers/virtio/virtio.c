/**
 * @file virtio.c
 * @brief VirtIO common implementation
 */

#include "../../include/drivers/virtio.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "../../include/mm/heap.h"
#include "../../include/mm/pmm.h"
#include "../../include/mm/vmm.h"
#include "../../include/string.h"

/**
 * Read VirtIO MMIO register (32-bit)
 */
static inline uint32_t virtio_mmio_read(uint64_t base, uint32_t offset) {
    volatile uint32_t* reg = (volatile uint32_t*)(base + offset);
    return *reg;
}

/**
 * Write VirtIO MMIO register (32-bit)
 */
static inline void virtio_mmio_write(uint64_t base, uint32_t offset, uint32_t value) {
    volatile uint32_t* reg = (volatile uint32_t*)(base + offset);
    *reg = value;
}

/**
 * Allocate queue memory (must be physically contiguous)
 */
void* virtio_alloc_queue_memory(uint16_t queue_size) {
    // Allocate contiguous memory for descriptors, available, and used rings
    size_t desc_size = 16 * queue_size;  // 16 bytes per descriptor
    size_t avail_size = 6 + 2 * queue_size;  // Header + ring
    size_t used_size = 6 + 8 * queue_size;   // Header + ring
    
    // Align to page size
    size_t total_size = ((desc_size + avail_size + used_size + 4095) / 4096) * 4096;
    
    // Allocate physically contiguous memory
    uint64_t phys_addr = pmm_alloc_pages((total_size + 4095) / 4096);
    if (!phys_addr) {
        return NULL;
    }
    
    // Map to virtual address
    void* virt_addr = (void*)((uint64_t)0xFFFF800000000000ULL + phys_addr);  // Use kernel mapping
    
    memset(virt_addr, 0, total_size);
    return virt_addr;
}

/**
 * Initialize VirtIO device
 */
error_code_t virtio_init(virtio_device_t* dev, uint64_t mmio_base) {
    if (!dev) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("Initializing VirtIO device at MMIO base 0x%llx\n", mmio_base);
    
    // Check magic value
    uint32_t magic = virtio_mmio_read(mmio_base, VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != 0x74726976) {  // "virt" in little-endian
        kerror("VirtIO: Invalid magic value 0x%x\n", magic);
        return ERR_INVALID_ARG;
    }
    
    // Check version
    uint32_t version = virtio_mmio_read(mmio_base, VIRTIO_MMIO_VERSION);
    if (version != 2) {
        kerror("VirtIO: Unsupported version %d (expected 2)\n", version);
        return ERR_NOT_SUPPORTED;
    }
    
    // Get device ID
    uint32_t device_id = virtio_mmio_read(mmio_base, VIRTIO_MMIO_DEVICE_ID);
    
    dev->mmio_base = mmio_base;
    dev->device_id = device_id;
    dev->version = version;
    dev->queues = NULL;
    dev->queue_count = 0;
    dev->initialized = false;
    
    // Reset device
    virtio_mmio_write(mmio_base, VIRTIO_MMIO_STATUS, 0);
    
    // Set ACKNOWLEDGE
    virtio_mmio_write(mmio_base, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    
    // Set DRIVER
    virtio_mmio_write(mmio_base, VIRTIO_MMIO_STATUS, 
                      VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);
    
    kinfo("VirtIO device initialized: ID=0x%x\n", device_id);
    
    return ERR_OK;
}

/**
 * Initialize VirtIO queue
 */
error_code_t virtio_queue_init(virtio_device_t* dev, uint16_t queue_index, uint16_t queue_size) {
    if (!dev || !dev->initialized) {
        return ERR_INVALID_STATE;
    }
    
    // Select queue
    virtio_mmio_write(dev->mmio_base, VIRTIO_MMIO_QUEUE_SEL, queue_index);
    
    // Check max queue size
    uint32_t max_size = virtio_mmio_read(dev->mmio_base, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (queue_size > max_size) {
        queue_size = max_size;
    }
    
    // Allocate queue memory
    void* queue_mem = virtio_alloc_queue_memory(queue_size);
    if (!queue_mem) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Set up queue
    virtio_mmio_write(dev->mmio_base, VIRTIO_MMIO_QUEUE_NUM, queue_size);
    
    // Get physical address
    uint64_t phys_addr = vmm_get_physical((uint64_t)queue_mem);
    
    // Set descriptor table address
    virtio_mmio_write(dev->mmio_base, VIRTIO_MMIO_QUEUE_DESC_LOW, phys_addr & 0xFFFFFFFF);
    virtio_mmio_write(dev->mmio_base, VIRTIO_MMIO_QUEUE_DESC_HIGH, phys_addr >> 32);
    
    // Set available ring address (after descriptors)
    uint64_t avail_addr = phys_addr + (16 * queue_size);
    virtio_mmio_write(dev->mmio_base, VIRTIO_MMIO_QUEUE_AVAIL_LOW, avail_addr & 0xFFFFFFFF);
    virtio_mmio_write(dev->mmio_base, VIRTIO_MMIO_QUEUE_AVAIL_HIGH, avail_addr >> 32);
    
    // Set used ring address (after available ring)
    uint64_t used_addr = avail_addr + (6 + 2 * queue_size);
    virtio_mmio_write(dev->mmio_base, VIRTIO_MMIO_QUEUE_USED_LOW, used_addr & 0xFFFFFFFF);
    virtio_mmio_write(dev->mmio_base, VIRTIO_MMIO_QUEUE_USED_HIGH, used_addr >> 32);
    
    // Mark queue as ready
    virtio_mmio_write(dev->mmio_base, VIRTIO_MMIO_QUEUE_READY, 1);
    
    kinfo("VirtIO queue %d initialized (size=%d)\n", queue_index, queue_size);
    
    return ERR_OK;
}

/**
 * Notify queue
 */
error_code_t virtio_queue_notify(virtio_device_t* dev, uint16_t queue_index) {
    if (!dev || !dev->initialized) {
        return ERR_INVALID_STATE;
    }
    
    virtio_mmio_write(dev->mmio_base, VIRTIO_MMIO_QUEUE_NOTIFY, queue_index);
    return ERR_OK;
}

