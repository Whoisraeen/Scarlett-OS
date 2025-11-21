/**
 * @file gpu.c
 * @brief GPU driver framework implementation
 */

#include "../../include/drivers/gpu/gpu.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "../../include/sync/spinlock.h"
#include "../../include/string.h"

// GPU framework state
static struct {
    gpu_device_t* devices;
    gpu_device_t* default_device;
    spinlock_t lock;
    bool initialized;
} gpu_state = {0};

/**
 * Initialize GPU framework
 */
error_code_t gpu_init(void) {
    if (gpu_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing GPU framework...\n");
    
    gpu_state.devices = NULL;
    gpu_state.default_device = NULL;
    spinlock_init(&gpu_state.lock);
    gpu_state.initialized = true;
    
    kinfo("GPU framework initialized\n");
    return ERR_OK;
}

/**
 * Register GPU device
 */
error_code_t gpu_register_device(gpu_device_t* device) {
    if (!device || !gpu_state.initialized) {
        return ERR_INVALID_ARG;
    }
    
    spinlock_lock(&gpu_state.lock);
    
    // Add to device list
    device->next = gpu_state.devices;
    gpu_state.devices = device;
    
    // Set as default if first device
    if (!gpu_state.default_device) {
        gpu_state.default_device = device;
    }
    
    spinlock_unlock(&gpu_state.lock);
    
    kinfo("GPU: Registered device %s (type: %d, capabilities: 0x%x)\n",
          device->name, device->type, device->capabilities);
    
    return ERR_OK;
}

/**
 * Find GPU device by name
 */
gpu_device_t* gpu_find_device(const char* name) {
    if (!name || !gpu_state.initialized) {
        return NULL;
    }
    
    spinlock_lock(&gpu_state.lock);
    
    gpu_device_t* device = gpu_state.devices;
    while (device) {
        if (strcmp(device->name, name) == 0) {
            spinlock_unlock(&gpu_state.lock);
            return device;
        }
        device = device->next;
    }
    
    spinlock_unlock(&gpu_state.lock);
    return NULL;
}

/**
 * Get default GPU device
 */
gpu_device_t* gpu_get_default(void) {
    if (!gpu_state.initialized) {
        return NULL;
    }
    
    return gpu_state.default_device;
}

/**
 * Set GPU mode
 */
error_code_t gpu_set_mode(gpu_device_t* gpu, const gpu_mode_t* mode) {
    if (!gpu || !mode) {
        return ERR_INVALID_ARG;
    }
    
    if (!gpu->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    if (gpu->set_mode) {
        return gpu->set_mode(gpu, mode);
    }
    
    return ERR_NOT_SUPPORTED;
}

/**
 * Submit GPU command
 */
error_code_t gpu_submit_command(gpu_device_t* gpu, const gpu_command_t* cmd) {
    if (!gpu || !cmd) {
        return ERR_INVALID_ARG;
    }
    
    if (!gpu->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    if (gpu->submit_command) {
        return gpu->submit_command(gpu, cmd);
    }
    
    return ERR_NOT_SUPPORTED;
}

/**
 * Flush GPU commands
 */
error_code_t gpu_flush(gpu_device_t* gpu) {
    if (!gpu) {
        return ERR_INVALID_ARG;
    }
    
    if (!gpu->initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    if (gpu->flush) {
        return gpu->flush(gpu);
    }
    
    return ERR_NOT_SUPPORTED;
}

/**
 * Get framebuffer
 */
void* gpu_get_framebuffer(gpu_device_t* gpu) {
    if (!gpu) {
        return NULL;
    }
    
    if (!gpu->initialized) {
        return NULL;
    }
    
    if (gpu->get_framebuffer) {
        return gpu->get_framebuffer(gpu);
    }
    
    return gpu->framebuffer;
}

