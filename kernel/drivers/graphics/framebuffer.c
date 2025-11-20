/**
 * @file framebuffer.c
 * @brief VESA/VBE framebuffer driver implementation
 */

#include "../../include/types.h"
#include "../../include/graphics/framebuffer.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
// VMM headers not needed here - framebuffer uses identity mapping from bootloader

// Global framebuffer instance
// NOTE: Explicitly initialized to move to .data section instead of .bss
// This prevents it from being zeroed after framebuffer_init() is called
static framebuffer_t g_framebuffer = {
    .base_address = NULL,
    .width = 0,
    .height = 0,
    .pitch = 0,
    .bpp = 0,
    .initialized = false
};

/**
 * Initialize framebuffer from boot info
 */
error_code_t framebuffer_init(framebuffer_info_t* boot_fb_info) {
    if (!boot_fb_info || boot_fb_info->base == 0) {
        kwarn("Framebuffer: No framebuffer information from bootloader\n");
        return ERR_DEVICE_NOT_FOUND;
    }
    
    kinfo("Initializing framebuffer...\n");
    kinfo("  Base: 0x%016lx\n", boot_fb_info->base);
    kinfo("  Resolution: %ux%u\n", boot_fb_info->width, boot_fb_info->height);
    kinfo("  BPP: %u\n", boot_fb_info->bpp);
    kinfo("  Pitch: %u bytes\n", boot_fb_info->pitch);
    
    // Copy framebuffer info
    g_framebuffer.base_address = (void*)boot_fb_info->base;
    g_framebuffer.width = boot_fb_info->width;
    g_framebuffer.height = boot_fb_info->height;
    g_framebuffer.pitch = boot_fb_info->pitch;
    g_framebuffer.bpp = boot_fb_info->bpp;
    g_framebuffer.red_mask = boot_fb_info->red_mask;
    g_framebuffer.green_mask = boot_fb_info->green_mask;
    g_framebuffer.blue_mask = boot_fb_info->blue_mask;
    g_framebuffer.reserved_mask = boot_fb_info->reserved_mask;
    g_framebuffer.initialized = true;
    
    // Note: Framebuffer is assumed to be identity-mapped by the bootloader.
    // VMM verification will happen later when VMM is initialized (Phase 2).
    // For now, we trust the bootloader's memory mapping.
    
    kinfo("Framebuffer initialized successfully. g_framebuffer at %p, initialized=%d\n", 
          &g_framebuffer, g_framebuffer.initialized);
    return ERR_OK;
}

/**
 * Get framebuffer instance
 */
framebuffer_t* framebuffer_get(void) {
    if (!g_framebuffer.initialized) {
        kwarn("framebuffer_get: g_framebuffer at %p is NOT initialized (val=%d)\n", 
              &g_framebuffer, g_framebuffer.initialized);
        return NULL;
    }
    return &g_framebuffer;
}

/**
 * Set a pixel at (x, y) to color
 */
void framebuffer_set_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!g_framebuffer.initialized) {
        return;
    }
    
    if (x >= g_framebuffer.width || y >= g_framebuffer.height) {
        return;  // Out of bounds
    }
    
    // Calculate pixel offset
    uint32_t bytes_per_pixel = g_framebuffer.bpp / 8;
    uint8_t* pixel_ptr = (uint8_t*)g_framebuffer.base_address;
    pixel_ptr += y * g_framebuffer.pitch + x * bytes_per_pixel;
    
    // Write pixel (assuming 32-bit RGBA)
    if (bytes_per_pixel == 4) {
        *(uint32_t*)pixel_ptr = color;
    } else if (bytes_per_pixel == 3) {
        // 24-bit RGB
        pixel_ptr[0] = (color >> 16) & 0xFF;  // R
        pixel_ptr[1] = (color >> 8) & 0xFF;   // G
        pixel_ptr[2] = color & 0xFF;          // B
    } else if (bytes_per_pixel == 2) {
        // 16-bit RGB565
        uint16_t r = ((color >> 16) & 0xFF) >> 3;
        uint16_t g = ((color >> 8) & 0xFF) >> 2;
        uint16_t b = (color & 0xFF) >> 3;
        *(uint16_t*)pixel_ptr = (r << 11) | (g << 5) | b;
    }
}

/**
 * Get pixel color at (x, y)
 */
uint32_t framebuffer_get_pixel(uint32_t x, uint32_t y) {
    if (!g_framebuffer.initialized) {
        return 0;
    }
    
    if (x >= g_framebuffer.width || y >= g_framebuffer.height) {
        return 0;  // Out of bounds
    }
    
    // Calculate pixel offset
    uint32_t bytes_per_pixel = g_framebuffer.bpp / 8;
    uint8_t* pixel_ptr = (uint8_t*)g_framebuffer.base_address;
    pixel_ptr += y * g_framebuffer.pitch + x * bytes_per_pixel;
    
    // Read pixel
    if (bytes_per_pixel == 4) {
        return *(uint32_t*)pixel_ptr;
    } else if (bytes_per_pixel == 3) {
        // 24-bit RGB
        uint32_t r = pixel_ptr[0];
        uint32_t g = pixel_ptr[1];
        uint32_t b = pixel_ptr[2];
        return RGB(r, g, b);
    } else if (bytes_per_pixel == 2) {
        // 16-bit RGB565
        uint16_t pixel = *(uint16_t*)pixel_ptr;
        uint32_t r = ((pixel >> 11) & 0x1F) << 3;
        uint32_t g = ((pixel >> 5) & 0x3F) << 2;
        uint32_t b = (pixel & 0x1F) << 3;
        return RGB(r, g, b);
    }
    
    return 0;
}

/**
 * Clear entire framebuffer with color
 */
void framebuffer_clear(uint32_t color) {
    if (!g_framebuffer.initialized) {
        return;
    }
    
    framebuffer_fill_rect(0, 0, g_framebuffer.width, g_framebuffer.height, color);
}

/**
 * Fill a rectangle with color
 */
void framebuffer_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    if (!g_framebuffer.initialized) {
        return;
    }
    
    // Clamp to framebuffer bounds
    if (x >= g_framebuffer.width || y >= g_framebuffer.height) {
        return;
    }
    
    if (x + width > g_framebuffer.width) {
        width = g_framebuffer.width - x;
    }
    if (y + height > g_framebuffer.height) {
        height = g_framebuffer.height - y;
    }
    
    uint32_t bytes_per_pixel = g_framebuffer.bpp / 8;
    
    // Optimized fill for 32-bit pixels
    if (bytes_per_pixel == 4) {
        uint32_t* row_start = (uint32_t*)((uint8_t*)g_framebuffer.base_address + y * g_framebuffer.pitch + x * 4);
        
        for (uint32_t py = 0; py < height; py++) {
            uint32_t* pixel = row_start;
            for (uint32_t px = 0; px < width; px++) {
                *pixel++ = color;
            }
            row_start = (uint32_t*)((uint8_t*)row_start + g_framebuffer.pitch);
        }
    } else {
        // Generic fill for other pixel formats
        for (uint32_t py = y; py < y + height; py++) {
            for (uint32_t px = x; px < x + width; px++) {
                framebuffer_set_pixel(px, py, color);
            }
        }
    }
}

