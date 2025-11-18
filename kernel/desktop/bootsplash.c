/**
 * @file bootsplash.c
 * @brief Boot splash screen implementation
 */

#include "../include/desktop/bootsplash.h"
#include "../include/graphics/graphics.h"
#include "../include/graphics/framebuffer.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/types.h"

// Boot splash state
static bootsplash_t bootsplash_state = {0};

/**
 * Initialize boot splash screen
 */
error_code_t bootsplash_init(void) {
    if (bootsplash_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing boot splash screen...\n");
    
    memset(&bootsplash_state, 0, sizeof(bootsplash_t));
    bootsplash_state.visible = true;
    bootsplash_state.progress = 0;
    strncpy(bootsplash_state.message, "Booting...", sizeof(bootsplash_state.message) - 1);
    bootsplash_state.message[sizeof(bootsplash_state.message) - 1] = '\0';
    bootsplash_state.initialized = true;
    
    kinfo("Boot splash screen initialized\n");
    return ERR_OK;
}

/**
 * Show boot splash
 */
error_code_t bootsplash_show(void) {
    if (!bootsplash_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    bootsplash_state.visible = true;
    return ERR_OK;
}

/**
 * Hide boot splash
 */
error_code_t bootsplash_hide(void) {
    if (!bootsplash_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    bootsplash_state.visible = false;
    return ERR_OK;
}

/**
 * Set boot splash message
 */
error_code_t bootsplash_set_message(const char* message) {
    if (!bootsplash_state.initialized || !message) {
        return ERR_INVALID_ARG;
    }
    
    strncpy(bootsplash_state.message, message, sizeof(bootsplash_state.message) - 1);
    bootsplash_state.message[sizeof(bootsplash_state.message) - 1] = '\0';
    
    return ERR_OK;
}

/**
 * Set boot splash progress
 */
error_code_t bootsplash_set_progress(uint32_t percent) {
    if (!bootsplash_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    if (percent > 100) {
        percent = 100;
    }
    
    bootsplash_state.progress = percent;
    return ERR_OK;
}

/**
 * Render boot splash with glassmorphism effect
 */
error_code_t bootsplash_render(void) {
    if (!bootsplash_state.initialized || !bootsplash_state.visible) {
        return ERR_OK;
    }
    
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return ERR_INVALID_STATE;
    }
    
    // Draw background (gradient - blue/purple)
    extern void framebuffer_set_pixel(uint32_t x, uint32_t y, uint32_t color);
    for (uint32_t y = 0; y < fb->height; y++) {
        for (uint32_t x = 0; x < fb->width; x++) {
            float t = (float)y / fb->height;
            uint8_t r = (uint8_t)(20 + (60 - 20) * t);
            uint8_t g = (uint8_t)(25 + (40 - 25) * t);
            uint8_t b = (uint8_t)(50 + (100 - 50) * t);
            framebuffer_set_pixel(x, y, RGB(r, g, b));
        }
    }
    
    // Logo/OS name (centered)
    uint32_t logo_y = fb->height / 3;
    gfx_draw_string(fb->width / 2 - 40, logo_y, "RaeenOS",
                   RGB(255, 255, 255), 0);
    
    // Message (below logo)
    uint32_t msg_y = logo_y + 48;
    gfx_draw_string(fb->width / 2 - (strlen(bootsplash_state.message) * 4), msg_y,
                   bootsplash_state.message,
                   RGB(200, 200, 220), 0);
    
    // Progress bar (glassmorphism style)
    uint32_t bar_x = fb->width / 4;
    uint32_t bar_y = fb->height / 2 + 32;
    uint32_t bar_w = fb->width / 2;
    uint32_t bar_h = 8;
    
    // Progress bar background (glass)
    gfx_draw_rect_alpha(bar_x, bar_y, bar_w, bar_h,
                       RGB(30, 30, 40), 200);
    gfx_draw_rect(bar_x, bar_y, bar_w, bar_h, RGB(80, 100, 120));
    
    // Progress bar fill
    uint32_t fill_w = (bar_w * bootsplash_state.progress) / 100;
    if (fill_w > 0) {
        gfx_fill_rect(bar_x + 1, bar_y + 1, fill_w - 2, bar_h - 2,
                     RGB(100, 150, 255));  // Cyan/blue glow
    }
    
    // Progress percentage
    char progress_str[32];
    // Simple format (would use sprintf in full implementation)
    if (bootsplash_state.progress < 10) {
        gfx_draw_string(bar_x + bar_w / 2 - 4, bar_y + 16, "  ",
                       RGB(255, 255, 255), 0);
    }
    // Display percentage text
    
    return ERR_OK;
}

/**
 * Get boot splash instance
 */
bootsplash_t* bootsplash_get(void) {
    return bootsplash_state.initialized ? &bootsplash_state : NULL;
}

