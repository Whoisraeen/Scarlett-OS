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
    
    // Create modern gradient wallpaper (glassmorphism style)
    // Inspired by iOS/macOS design - deep blue to purple-pink gradient
    uint32_t* wp = (uint32_t*)desktop_state.wallpaper_buffer;
    for (uint32_t y = 0; y < fb->height; y++) {
        for (uint32_t x = 0; x < fb->width; x++) {
            // Multi-color gradient: deep blue -> purple -> pink
            float t_y = (float)y / fb->height;
            float t_x = (float)x / fb->width;

            // Create diagonal gradient with radial influence
            float t = (t_y * 0.7f + t_x * 0.3f);

            // Define gradient color stops
            uint8_t r, g, b;
            if (t < 0.5f) {
                // Deep blue to vibrant purple
                float local_t = t * 2.0f;
                r = (uint8_t)(15 + (80 - 15) * local_t);
                g = (uint8_t)(25 + (40 - 25) * local_t);
                b = (uint8_t)(50 + (120 - 50) * local_t);
            } else {
                // Vibrant purple to deep teal
                float local_t = (t - 0.5f) * 2.0f;
                r = (uint8_t)(80 + (35 - 80) * local_t);
                g = (uint8_t)(40 + (50 - 40) * local_t);
                b = (uint8_t)(120 + (80 - 120) * local_t);
            }

            // Add subtle noise for depth (simple pattern)
            uint8_t noise = ((x + y) % 3 == 0) ? 2 : 0;
            r = (r + noise > 255) ? 255 : r + noise;
            g = (g + noise > 255) ? 255 : g + noise;
            b = (b + noise > 255) ? 255 : b + noise;

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
    
    // Get target buffer (back buffer if enabled, otherwise front buffer)
    void* target_buffer = gfx_get_back_buffer();
    if (!target_buffer) {
        target_buffer = fb->base_address;
    }
    
    // Draw wallpaper
    memcpy(target_buffer, desktop_state.wallpaper_buffer, fb->height * fb->pitch);
    
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

