/**
 * @file virtio_gpu_driver.c
 * @brief VirtIO GPU driver integration with GPU framework
 */

#include "../../include/drivers/gpu/gpu.h"
#include "../../include/drivers/virtio_gpu.h"
#include "../../include/drivers/virtio.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "../../include/mm/heap.h"
#include "../../include/string.h"

// VirtIO GPU device instance
static gpu_device_t virtio_gpu_device = {0};
static virtio_gpu_t* g_virtio_gpu = NULL;

/**
 * VirtIO GPU init function
 */
static error_code_t virtio_gpu_driver_init(gpu_device_t* gpu) {
    if (!gpu) {
        return ERR_INVALID_ARG;
    }
    
    // Get VirtIO GPU instance
    extern virtio_gpu_t* virtio_gpu_get(void);
    virtio_gpu_t* virtio_gpu = virtio_gpu_get();
    
    if (!virtio_gpu || !virtio_gpu->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    g_virtio_gpu = virtio_gpu;
    gpu->initialized = true;
    
    kinfo("VirtIO GPU driver: Initialized\n");
    return ERR_OK;
}

/**
 * VirtIO GPU set mode function
 */
static error_code_t virtio_gpu_driver_set_mode(gpu_device_t* gpu, const gpu_mode_t* mode) {
    if (!gpu || !mode || !g_virtio_gpu) {
        return ERR_INVALID_ARG;
    }
    
    // Create surface with new mode
    error_code_t err = virtio_gpu_create_surface(g_virtio_gpu, mode->width, mode->height);
    if (err != ERR_OK) {
        return err;
    }
    
    gpu->current_mode = *mode;
    gpu->framebuffer = (uint32_t*)virtio_gpu_get_framebuffer(g_virtio_gpu);
    gpu->framebuffer_size = mode->width * mode->height * (mode->bpp / 8);
    
    kinfo("VirtIO GPU driver: Mode set to %ux%u@%ubpp\n",
          mode->width, mode->height, mode->bpp);
    
    return ERR_OK;
}

/**
 * VirtIO GPU submit command function
 */
static error_code_t virtio_gpu_driver_submit_command(gpu_device_t* gpu, const gpu_command_t* cmd) {
    if (!gpu || !cmd || !g_virtio_gpu) {
        return ERR_INVALID_ARG;
    }
    
    switch (cmd->type) {
        case GPU_CMD_CLEAR:
            // Clear framebuffer
            if (gpu->framebuffer) {
                uint32_t* fb = gpu->framebuffer;
                size_t pixel_count = gpu->current_mode.width * gpu->current_mode.height;
                for (size_t i = 0; i < pixel_count; i++) {
                    fb[i] = cmd->color;
                }
            }
            return ERR_OK;
            
        case GPU_CMD_FLUSH:
            // Flush to display
            return virtio_gpu_flush(g_virtio_gpu, cmd->x, cmd->y, cmd->width, cmd->height);
            
        case GPU_CMD_DRAW_RECT:
            // Draw rectangle using framebuffer directly
            if (!gpu->framebuffer) {
                return ERR_NOT_INITIALIZED;
            }
            
            // Draw rectangle outline using cmd parameters
            uint32_t* fb = (uint32_t*)gpu->framebuffer;
            uint32_t width = gpu->current_mode.width;
            uint32_t height = gpu->current_mode.height;
            uint32_t color = cmd->color;
            
            // Extract parameters from cmd structure
            uint32_t rect_x = cmd->x;
            uint32_t rect_y = cmd->y;
            uint32_t rect_w = cmd->width;
            uint32_t rect_h = cmd->height;
            
            // Clamp to framebuffer bounds
            if (rect_x >= width || rect_y >= height) {
                return ERR_OK;  // Rectangle is outside framebuffer
            }
            
            if (rect_x + rect_w > width) {
                rect_w = width - rect_x;
            }
            if (rect_y + rect_h > height) {
                rect_h = height - rect_y;
            }
            
            // Top edge
            for (uint32_t i = rect_x; i < rect_x + rect_w; i++) {
                fb[rect_y * width + i] = color;
            }
            
            // Bottom edge
            if (rect_h > 1) {
                uint32_t bottom_y = rect_y + rect_h - 1;
                for (uint32_t i = rect_x; i < rect_x + rect_w; i++) {
                    fb[bottom_y * width + i] = color;
                }
            }
            
            // Left edge
            for (uint32_t i = rect_y; i < rect_y + rect_h; i++) {
                fb[i * width + rect_x] = color;
            }
            
            // Right edge
            if (rect_w > 1) {
                uint32_t right_x = rect_x + rect_w - 1;
                for (uint32_t i = rect_y; i < rect_y + rect_h; i++) {
                    fb[i * width + right_x] = color;
                }
            }
            
            return ERR_OK;
            
        default:
            return ERR_NOT_SUPPORTED;
    }
}

/**
 * VirtIO GPU flush function
 */
static error_code_t virtio_gpu_driver_flush(gpu_device_t* gpu) {
    if (!gpu || !g_virtio_gpu) {
        return ERR_INVALID_ARG;
    }
    
    if (gpu->current_mode.width > 0 && gpu->current_mode.height > 0) {
        return virtio_gpu_flush(g_virtio_gpu, 0, 0, gpu->current_mode.width, gpu->current_mode.height);
    }
    
    return ERR_OK;
}

/**
 * VirtIO GPU get framebuffer function
 */
static void* virtio_gpu_driver_get_framebuffer(gpu_device_t* gpu) {
    if (!gpu || !g_virtio_gpu) {
        return NULL;
    }
    
    return virtio_gpu_get_framebuffer(g_virtio_gpu);
}

/**
 * Register VirtIO GPU with GPU framework
 */
error_code_t virtio_gpu_register_with_framework(void) {
    // Check if VirtIO GPU exists
    extern virtio_gpu_t* virtio_gpu_get(void);
    virtio_gpu_t* virtio_gpu = virtio_gpu_get();
    
    if (!virtio_gpu || !virtio_gpu->initialized) {
        kinfo("VirtIO GPU: Not available, skipping registration\n");
        return ERR_NOT_INITIALIZED;
    }
    
    // Initialize GPU device structure
    memset(&virtio_gpu_device, 0, sizeof(gpu_device_t));
    
    virtio_gpu_device.type = GPU_TYPE_VIRTIO;
    strncpy(virtio_gpu_device.name, "virtio-gpu", sizeof(virtio_gpu_device.name) - 1);
    virtio_gpu_device.capabilities = GPU_CAP_2D;
    virtio_gpu_device.framebuffer = (uint32_t*)virtio_gpu_get_framebuffer(virtio_gpu);
    virtio_gpu_device.initialized = false;
    
    // Set driver functions
    virtio_gpu_device.init = virtio_gpu_driver_init;
    virtio_gpu_device.set_mode = virtio_gpu_driver_set_mode;
    virtio_gpu_device.submit_command = virtio_gpu_driver_submit_command;
    virtio_gpu_device.flush = virtio_gpu_driver_flush;
    virtio_gpu_device.get_framebuffer = virtio_gpu_driver_get_framebuffer;
    virtio_gpu_device.deinit = NULL;  // Not needed for now
    
    // Register with GPU framework
    extern error_code_t gpu_register_device(gpu_device_t* device);
    error_code_t err = gpu_register_device(&virtio_gpu_device);
    
    if (err == ERR_OK) {
        // Initialize the device
        virtio_gpu_device.init(&virtio_gpu_device);
        
        // Set default mode
        gpu_mode_t default_mode = {
            .width = virtio_gpu->width ? virtio_gpu->width : 1024,
            .height = virtio_gpu->height ? virtio_gpu->height : 768,
            .bpp = 32,
            .refresh_rate = 60
        };
        virtio_gpu_device.set_mode(&virtio_gpu_device, &default_mode);
        
        kinfo("VirtIO GPU: Registered with GPU framework\n");
    }
    
    return err;
}

