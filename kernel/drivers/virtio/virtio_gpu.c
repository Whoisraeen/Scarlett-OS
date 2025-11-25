/**
 * @file virtio_gpu.c
 * @brief VirtIO GPU driver implementation
 */

#include "../../include/drivers/virtio_gpu.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "../../include/mm/heap.h"
#include "../../include/mm/pmm.h"
#include "../../include/mm/vmm.h"
#include "../../include/string.h"

// Static VirtIO GPU instance
static virtio_gpu_t* g_virtio_gpu = NULL;

/**
 * Send command to VirtIO GPU
 */
static error_code_t virtio_gpu_send_command(virtio_gpu_t* gpu, void* cmd, size_t cmd_size __attribute__((unused))) {
    if (!gpu || !gpu->virtio_dev || !gpu->virtio_dev->initialized) {
        return ERR_INVALID_STATE;
    }
    
    // Simplified command sending
    // In full implementation, would use virtqueue descriptors
    
    // For now, just acknowledge command
    virtio_gpu_ctrl_hdr_t* hdr = (virtio_gpu_ctrl_hdr_t*)cmd;
    
    kinfo("VirtIO GPU: Sending command 0x%x\n", hdr->type);
    
    return ERR_OK;
}

/**
 * Initialize VirtIO GPU
 */
error_code_t virtio_gpu_init(virtio_gpu_t* gpu, uint64_t mmio_base) {
    if (!gpu) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("Initializing VirtIO GPU...\n");
    
    // Allocate VirtIO device structure
    virtio_device_t* virtio_dev = (virtio_device_t*)kmalloc(sizeof(virtio_device_t));
    if (!virtio_dev) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Initialize VirtIO device
    error_code_t err = virtio_init(virtio_dev, mmio_base);
    if (err != ERR_OK) {
        kfree(virtio_dev);
        return err;
    }
    
    // Check if it's a GPU device
    if (virtio_dev->device_id != VIRTIO_DEVICE_ID_GPU) {
        kerror("VirtIO: Not a GPU device (ID=0x%x)\n", virtio_dev->device_id);
        kfree(virtio_dev);
        return ERR_NOT_SUPPORTED;
    }
    
    // Initialize queues
    err = virtio_queue_init(virtio_dev, VIRTIO_GPU_CONTROL_QUEUE, 64);
    if (err != ERR_OK) {
        kfree(virtio_dev);
        return err;
    }
    
    virtio_dev->initialized = true;
    
    // Set up GPU structure
    memset(gpu, 0, sizeof(virtio_gpu_t));
    gpu->virtio_dev = virtio_dev;
    gpu->resource_id = 1;  // Start with resource ID 1
    gpu->initialized = true;
    g_virtio_gpu = gpu;
    
    kinfo("VirtIO GPU initialized\n");
    
    return ERR_OK;
}

/**
 * Get display information
 */
error_code_t virtio_gpu_get_display_info(virtio_gpu_t* gpu) {
    if (!gpu || !gpu->initialized) {
        return ERR_INVALID_STATE;
    }
    
    virtio_gpu_ctrl_hdr_t cmd = {0};
    cmd.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    cmd.flags = 0;
    cmd.fence_id = 0;
    cmd.ctx_id = 0;
    
    error_code_t err = virtio_gpu_send_command(gpu, &cmd, sizeof(cmd));
    if (err != ERR_OK) {
        return err;
    }
    
    // In full implementation, would wait for response and parse it
    // For now, use default resolution
    gpu->width = 1024;
    gpu->height = 768;
    
    kinfo("VirtIO GPU: Display %dx%d\n", gpu->width, gpu->height);
    
    return ERR_OK;
}

/**
 * Create surface/resource
 */
error_code_t virtio_gpu_create_surface(virtio_gpu_t* gpu, uint32_t width, uint32_t height) {
    if (!gpu || !gpu->initialized) {
        return ERR_INVALID_STATE;
    }
    
    // Allocate framebuffer
    size_t fb_size = width * height * 4;  // 32-bit RGBA
    uint64_t phys_addr = pmm_alloc_pages((fb_size + 4095) / 4096);
    if (!phys_addr) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Map to virtual address
    void* fb_virt = (void*)(0xFFFF800000000000ULL + phys_addr);
    memset(fb_virt, 0, fb_size);
    
    // Create resource
    virtio_gpu_resource_create_2d_t create_cmd = {0};
    create_cmd.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    create_cmd.resource_id = gpu->resource_id;
    create_cmd.format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
    create_cmd.width = width;
    create_cmd.height = height;
    
    error_code_t err = virtio_gpu_send_command(gpu, &create_cmd, sizeof(create_cmd));
    if (err != ERR_OK) {
        pmm_free_pages(phys_addr, (fb_size + 4095) / 4096);
        return err;
    }
    
    // Attach backing store
    virtio_gpu_resource_attach_backing_t attach_cmd = {0};
    attach_cmd.hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    attach_cmd.resource_id = gpu->resource_id;
    attach_cmd.nr_entries = 1;
    attach_cmd.entries[0].addr = phys_addr;
    attach_cmd.entries[0].length = fb_size;
    
    err = virtio_gpu_send_command(gpu, &attach_cmd, sizeof(attach_cmd) + sizeof(attach_cmd.entries[0]));
    if (err != ERR_OK) {
        pmm_free_pages(phys_addr, (fb_size + 4095) / 4096);
        return err;
    }
    
    // Set scanout
    virtio_gpu_set_scanout_t scanout_cmd = {0};
    scanout_cmd.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    scanout_cmd.scanout_id = 0;
    scanout_cmd.resource_id = gpu->resource_id;
    scanout_cmd.r.x = 0;
    scanout_cmd.r.y = 0;
    scanout_cmd.r.width = width;
    scanout_cmd.r.height = height;
    
    err = virtio_gpu_send_command(gpu, &scanout_cmd, sizeof(scanout_cmd));
    if (err != ERR_OK) {
        pmm_free_pages(phys_addr, (fb_size + 4095) / 4096);
        return err;
    }
    
    gpu->width = width;
    gpu->height = height;
    gpu->framebuffer = fb_virt;
    
    kinfo("VirtIO GPU: Surface created %dx%d (resource_id=%d)\n", width, height, gpu->resource_id);
    
    return ERR_OK;
}

/**
 * Flush surface to display
 */
error_code_t virtio_gpu_flush(virtio_gpu_t* gpu, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (!gpu || !gpu->initialized || !gpu->framebuffer) {
        return ERR_INVALID_STATE;
    }
    
    // Transfer to host
    virtio_gpu_transfer_to_host_2d_t transfer_cmd = {0};
    transfer_cmd.hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    transfer_cmd.resource_id = gpu->resource_id;
    transfer_cmd.r.x = x;
    transfer_cmd.r.y = y;
    transfer_cmd.r.width = width;
    transfer_cmd.r.height = height;
    transfer_cmd.offset = 0;
    
    error_code_t err = virtio_gpu_send_command(gpu, &transfer_cmd, sizeof(transfer_cmd));
    if (err != ERR_OK) {
        return err;
    }
    
    // Flush resource
    virtio_gpu_resource_flush_t flush_cmd = {0};
    flush_cmd.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    flush_cmd.resource_id = gpu->resource_id;
    flush_cmd.r.x = x;
    flush_cmd.r.y = y;
    flush_cmd.r.width = width;
    flush_cmd.r.height = height;
    
    return virtio_gpu_send_command(gpu, &flush_cmd, sizeof(flush_cmd));
}

/**
 * Get framebuffer address
 */
void* virtio_gpu_get_framebuffer(virtio_gpu_t* gpu) {
    if (!gpu || !gpu->initialized) {
        return NULL;
    }
    return gpu->framebuffer;
}

/**
 * Create 3D context
 */
error_code_t virtio_gpu_ctx_create(virtio_gpu_t* gpu, uint32_t ctx_id, const char* name) {
    if (!gpu || !gpu->initialized) {
        return ERR_INVALID_STATE;
    }
    
    virtio_gpu_ctx_create_t cmd = {0};
    cmd.hdr.type = VIRTIO_GPU_CMD_CTX_CREATE;
    cmd.hdr.ctx_id = ctx_id;
    cmd.nlen = 0;
    if (name) {
        size_t len = strlen(name);
        if (len > 63) len = 63;
        memcpy(cmd.debug_name, name, len);
        cmd.debug_name[len] = '\0';
        cmd.nlen = len;
    }
    
    return virtio_gpu_send_command(gpu, &cmd, sizeof(cmd));
}

/**
 * Destroy 3D context
 */
error_code_t virtio_gpu_ctx_destroy(virtio_gpu_t* gpu, uint32_t ctx_id) {
    if (!gpu || !gpu->initialized) {
        return ERR_INVALID_STATE;
    }
    
    virtio_gpu_ctx_destroy_t cmd = {0};
    cmd.hdr.type = VIRTIO_GPU_CMD_CTX_DESTROY;
    cmd.hdr.ctx_id = ctx_id;
    
    return virtio_gpu_send_command(gpu, &cmd, sizeof(cmd));
}

/**
 * Submit 3D commands
 */
error_code_t virtio_gpu_submit_3d(virtio_gpu_t* gpu, uint32_t ctx_id, void* cmd_buf, size_t size) {
    if (!gpu || !gpu->initialized || !cmd_buf || size == 0) {
        return ERR_INVALID_ARG;
    }
    
    // Allocate buffer for header + command buffer
    size_t total_size = sizeof(virtio_gpu_cmd_submit_t) + size;
    uint8_t* buf = (uint8_t*)kmalloc(total_size);
    if (!buf) {
        return ERR_OUT_OF_MEMORY;
    }
    
    virtio_gpu_cmd_submit_t* cmd = (virtio_gpu_cmd_submit_t*)buf;
    cmd->hdr.type = VIRTIO_GPU_CMD_SUBMIT_3D;
    cmd->hdr.ctx_id = ctx_id;
    cmd->size = size;
    
    // Copy command buffer
    memcpy(buf + sizeof(virtio_gpu_cmd_submit_t), cmd_buf, size);
    
    error_code_t err = virtio_gpu_send_command(gpu, buf, total_size);
    
    kfree(buf);
    return err;
}

/**
 * Get VirtIO GPU instance
 */
virtio_gpu_t* virtio_gpu_get(void) {
    return g_virtio_gpu;
}

