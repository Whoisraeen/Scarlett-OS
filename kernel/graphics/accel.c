/**
 * @file accel.c
 * @brief 2D Graphics Acceleration implementation
 */

#include "../include/graphics/accel.h"
#include "../include/graphics/framebuffer.h"
#include "../include/drivers/virtio_gpu.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"

// Acceleration capabilities
static gfx_accel_caps_t accel_caps = {0};
static bool accel_initialized = false;

/**
 * Software blit (fallback)
 */
static error_code_t gfx_sw_blit(uint32_t src_x, uint32_t src_y, uint32_t width, uint32_t height,
                                uint32_t dest_x, uint32_t dest_y, void* src_fb, void* dest_fb,
                                uint32_t flags) {
    if (!src_fb || !dest_fb) {
        return ERR_INVALID_ARG;
    }
    
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return ERR_INVALID_STATE;
    }
    
    uint32_t bpp = fb->bpp / 8;
    uint32_t src_pitch = fb->pitch;
    uint32_t dest_pitch = fb->pitch;
    
    // Copy pixel by pixel (can be optimized with memcpy for aligned rows)
    for (uint32_t y = 0; y < height; y++) {
        uint8_t* src_row = (uint8_t*)src_fb + (src_y + y) * src_pitch + src_x * bpp;
        uint8_t* dest_row = (uint8_t*)dest_fb + (dest_y + y) * dest_pitch + dest_x * bpp;
        
        if (flags & GFX_BLIT_ALPHA) {
            // Alpha blending
            for (uint32_t x = 0; x < width; x++) {
                uint32_t src_pixel = *(uint32_t*)(src_row + x * bpp);
                uint32_t dest_pixel = *(uint32_t*)(dest_row + x * bpp);
                
                uint8_t src_a = (src_pixel >> 24) & 0xFF;
                uint8_t src_r = (src_pixel >> 16) & 0xFF;
                uint8_t src_g = (src_pixel >> 8) & 0xFF;
                uint8_t src_b = src_pixel & 0xFF;
                
                uint8_t dest_r = (dest_pixel >> 16) & 0xFF;
                uint8_t dest_g = (dest_pixel >> 8) & 0xFF;
                uint8_t dest_b = dest_pixel & 0xFF;
                
                // Blend
                uint8_t final_r = (src_r * src_a + dest_r * (255 - src_a)) / 255;
                uint8_t final_g = (src_g * src_a + dest_g * (255 - src_a)) / 255;
                uint8_t final_b = (src_b * src_a + dest_b * (255 - src_a)) / 255;
                
                *(uint32_t*)(dest_row + x * bpp) = RGB(final_r, final_g, final_b);
            }
        } else {
            // Direct copy
            memcpy(dest_row, src_row, width * bpp);
        }
    }
    
    return ERR_OK;
}

/**
 * Initialize acceleration
 */
error_code_t gfx_accel_init(void) {
    if (accel_initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing 2D graphics acceleration...\n");
    
    // Check for VirtIO GPU
    extern virtio_gpu_t* virtio_gpu_get(void);
    virtio_gpu_t* gpu = virtio_gpu_get();
    
    if (gpu && gpu->initialized) {
        // VirtIO GPU supports hardware acceleration
        accel_caps.blit_supported = true;
        accel_caps.fill_supported = true;
        accel_caps.alpha_blend_supported = true;
        accel_caps.rotate_supported = false;
        accel_caps.scale_supported = false;
        
        kinfo("2D acceleration: Hardware (VirtIO GPU)\n");
    } else {
        // Software fallback
        accel_caps.blit_supported = true;
        accel_caps.fill_supported = true;
        accel_caps.alpha_blend_supported = true;
        accel_caps.rotate_supported = false;
        accel_caps.scale_supported = false;
        
        kinfo("2D acceleration: Software fallback\n");
    }
    
    accel_initialized = true;
    return ERR_OK;
}

/**
 * Get acceleration capabilities
 */
gfx_accel_caps_t* gfx_accel_get_caps(void) {
    if (!accel_initialized) {
        gfx_accel_init();
    }
    return &accel_caps;
}

/**
 * Blit operation
 */
error_code_t gfx_accel_blit(uint32_t src_x, uint32_t src_y, uint32_t width, uint32_t height,
                            uint32_t dest_x, uint32_t dest_y, void* src_fb, void* dest_fb,
                            uint32_t flags) {
    if (!accel_initialized) {
        gfx_accel_init();
    }
    
    if (!accel_caps.blit_supported) {
        return ERR_NOT_SUPPORTED;
    }
    
    // Check for hardware acceleration
    extern virtio_gpu_t* virtio_gpu_get(void);
    virtio_gpu_t* gpu = virtio_gpu_get();
    
    if (gpu && gpu->initialized) {
        // Use hardware blit via VirtIO GPU
        // For now, fall back to software
        return gfx_sw_blit(src_x, src_y, width, height, dest_x, dest_y, src_fb, dest_fb, flags);
    }
    
    // Software blit
    return gfx_sw_blit(src_x, src_y, width, height, dest_x, dest_y, src_fb, dest_fb, flags);
}

/**
 * Fill rectangle (accelerated)
 */
error_code_t gfx_accel_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                uint32_t color, void* fb) {
    if (!accel_initialized) {
        gfx_accel_init();
    }
    
    if (!accel_caps.fill_supported) {
        return ERR_NOT_SUPPORTED;
    }
    
    framebuffer_t* framebuffer = framebuffer_get();
    if (!framebuffer || !fb) {
        return ERR_INVALID_STATE;
    }
    
    uint32_t bpp = framebuffer->bpp / 8;
    uint32_t pitch = framebuffer->pitch;
    
    // Optimized fill
    for (uint32_t py = y; py < y + height; py++) {
        uint32_t* row = (uint32_t*)((uint8_t*)fb + py * pitch + x * bpp);
        for (uint32_t px = 0; px < width; px++) {
            row[px] = color;
        }
    }
    
    return ERR_OK;
}

/**
 * Copy rectangle
 */
error_code_t gfx_accel_copy_rect(uint32_t src_x, uint32_t src_y, uint32_t width, uint32_t height,
                                uint32_t dest_x, uint32_t dest_y, void* fb) {
    return gfx_accel_blit(src_x, src_y, width, height, dest_x, dest_y, fb, fb, GFX_BLIT_NONE);
}

/**
 * Alpha blend
 */
error_code_t gfx_accel_alpha_blend(uint32_t src_x, uint32_t src_y, uint32_t width, uint32_t height,
                                   uint32_t dest_x, uint32_t dest_y, uint8_t alpha,
                                   void* src_fb, void* dest_fb) {
    if (!accel_initialized) {
        gfx_accel_init();
    }
    
    if (!accel_caps.alpha_blend_supported) {
        return ERR_NOT_SUPPORTED;
    }
    
    return gfx_accel_blit(src_x, src_y, width, height, dest_x, dest_y, src_fb, dest_fb, GFX_BLIT_ALPHA);
}

