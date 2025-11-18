/**
 * @file desktop.c
 * @brief Desktop environment implementation
 */

#include "../include/desktop/desktop.h"
#include "../include/desktop/taskbar.h"
#include "../include/desktop/launcher.h"
#include "../include/graphics/graphics.h"
#include "../include/graphics/framebuffer.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"
#include "../include/types.h"

// Desktop state
static desktop_state_t desktop_state = {0};

/**
 * Initialize desktop environment
 */
error_code_t desktop_init(void) {
    if (desktop_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing desktop environment...\n");
    
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return ERR_INVALID_STATE;
    }
    
    // Allocate wallpaper buffer (same size as framebuffer)
    size_t wallpaper_size = fb->height * fb->pitch;
    desktop_state.wallpaper_buffer = kmalloc(wallpaper_size);
    if (!desktop_state.wallpaper_buffer) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Create gradient wallpaper (glassmorphism style - blue/purple gradient)
    uint32_t* wp = (uint32_t*)desktop_state.wallpaper_buffer;
    for (uint32_t y = 0; y < fb->height; y++) {
        for (uint32_t x = 0; x < fb->width; x++) {
            // Blue to purple gradient
            float t = (float)y / fb->height;
            uint8_t r = (uint8_t)(20 + (80 - 20) * t);
            uint8_t g = (uint8_t)(30 + (50 - 30) * t);
            uint8_t b = (uint8_t)(60 + (120 - 60) * t);
            wp[y * (fb->pitch / 4) + x] = RGB(r, g, b);
        }
    }
    
    desktop_state.initialized = true;
    
    kinfo("Desktop environment initialized\n");
    return ERR_OK;
}

/**
 * Render desktop
 */
error_code_t desktop_render(void) {
    if (!desktop_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return ERR_INVALID_STATE;
    }
    
    // Draw wallpaper
    memcpy(fb->base_address, desktop_state.wallpaper_buffer, fb->height * fb->pitch);
    
    return ERR_OK;
}

/**
 * Handle desktop input
 */
error_code_t desktop_handle_input(void) {
    if (!desktop_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    // Desktop handles click events (for desktop icons, etc.)
    // For now, this is a placeholder
    
    return ERR_OK;
}

