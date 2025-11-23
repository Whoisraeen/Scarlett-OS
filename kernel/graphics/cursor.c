/**
 * @file cursor.c
 * @brief Mouse cursor rendering implementation
 */

#include "../include/graphics/cursor.h"
#include "../include/graphics/graphics.h"
#include "../include/graphics/framebuffer.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/math.h"
#include "../include/time.h"

// Cursor state
static cursor_t g_cursor = {
    .type = CURSOR_ARROW,
    .x = 0,
    .y = 0,
    .visible = true,
    .hot_x = 0,
    .hot_y = 0
};

// Arrow cursor bitmap (16x16, white with black outline)
static const uint16_t cursor_arrow_bitmap[16] = {
    0x8000, 0xC000, 0xE000, 0xF000, 0xF800, 0xFC00, 0xFE00, 0xFF00,
    0xFF80, 0xFC00, 0xDC00, 0x8E00, 0x0700, 0x0200, 0x0000, 0x0000
};

static const uint16_t cursor_arrow_mask[16] = {
    0xC000, 0xE000, 0xF000, 0xF800, 0xFC00, 0xFE00, 0xFF00, 0xFF80,
    0xFFC0, 0xFFE0, 0xFFE0, 0xFFF0, 0x7FF8, 0x3FFC, 0x1FFE, 0x0FFF
};

// Text cursor (I-beam, 2x16)
static const uint16_t cursor_text_bitmap[16] = {
    0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800,
    0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800
};

// Hand cursor (16x16)
static const uint16_t cursor_hand_bitmap[16] = {
    0x0000, 0x0000, 0x1800, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00,
    0x3C00, 0x7E00, 0x7E00, 0x7E00, 0x7E00, 0x7E00, 0x0000, 0x0000
};

// Horizontal resize cursor (16x16)
static const uint16_t cursor_resize_h_bitmap[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

// Vertical resize cursor (16x16)
static const uint16_t cursor_resize_v_bitmap[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

// Crosshair cursor (16x16)
static const uint16_t cursor_crosshair_bitmap[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

/**
 * Initialize cursor system
 */
void cursor_init(void) {
    kinfo("Initializing cursor system...\n");
    g_cursor.type = CURSOR_ARROW;
    g_cursor.x = 0;
    g_cursor.y = 0;
    g_cursor.visible = true;
    g_cursor.hot_x = 0;
    g_cursor.hot_y = 0;
    kinfo("Cursor system initialized\n");
}

/**
 * Set cursor type
 */
void cursor_set_type(cursor_type_t type) {
    g_cursor.type = type;
    
    // Set hotspot based on cursor type
    switch (type) {
        case CURSOR_ARROW:
            g_cursor.hot_x = 0;
            g_cursor.hot_y = 0;
            break;
        case CURSOR_TEXT:
            g_cursor.hot_x = 1;
            g_cursor.hot_y = 8;
            break;
        case CURSOR_HAND:
            g_cursor.hot_x = 8;
            g_cursor.hot_y = 8;
            break;
        case CURSOR_RESIZE_H:
        case CURSOR_RESIZE_V:
        case CURSOR_RESIZE_DIAG1:
        case CURSOR_RESIZE_DIAG2:
            g_cursor.hot_x = 8;
            g_cursor.hot_y = 8;
            break;
        case CURSOR_WAIT:
        case CURSOR_CROSSHAIR:
            g_cursor.hot_x = 8;
            g_cursor.hot_y = 8;
            break;
        case CURSOR_NONE:
            break;
    }
}

/**
 * Get current cursor type
 */
cursor_type_t cursor_get_type(void) {
    return g_cursor.type;
}

/**
 * Set cursor position
 */
void cursor_set_position(uint32_t x, uint32_t y) {
    g_cursor.x = x;
    g_cursor.y = y;
}

/**
 * Get cursor position
 */
void cursor_get_position(uint32_t* x, uint32_t* y) {
    if (x) *x = g_cursor.x;
    if (y) *y = g_cursor.y;
}

/**
 * Show cursor
 */
void cursor_show(void) {
    g_cursor.visible = true;
}

/**
 * Hide cursor
 */
void cursor_hide(void) {
    g_cursor.visible = false;
}

/**
 * Check if cursor is visible
 */
bool cursor_is_visible(void) {
    return g_cursor.visible;
}

/**
 * Render a 16x16 bitmap cursor
 */
static void render_bitmap_cursor(uint32_t x, uint32_t y, const uint16_t* bitmap, const uint16_t* mask, uint32_t fg_color, uint32_t bg_color) {
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return;
    }
    
    for (uint32_t row = 0; row < 16; row++) {
        uint16_t bit_row = bitmap[row];
        uint16_t mask_row = mask ? mask[row] : 0xFFFF;
        
        for (uint32_t col = 0; col < 16; col++) {
            if (mask_row & (1 << (15 - col))) {
                uint32_t px = x + col;
                uint32_t py = y + row;
                
                if (bit_row & (1 << (15 - col))) {
                    // Draw foreground pixel
                    gfx_fill_rect(px, py, 1, 1, fg_color);
                } else {
                    // Draw background pixel (for XOR effect)
                    gfx_fill_rect(px, py, 1, 1, bg_color);
                }
            }
        }
    }
}

/**
 * Render text cursor (I-beam)
 */
static void render_text_cursor(uint32_t x, uint32_t y) {
    // Simple vertical line
    gfx_draw_line(x, y, x, y + 16, 0xFFFFFF);  // White
    gfx_draw_line(x + 1, y, x + 1, y + 16, 0x000000);  // Black outline
}

/**
 * Render horizontal resize cursor
 */
static void render_resize_h_cursor(uint32_t x, uint32_t y) {
    // Horizontal double arrow
    uint32_t center_y = y + 8;
    gfx_draw_line(x, center_y, x + 16, center_y, 0xFFFFFF);
    // Left arrow
    gfx_draw_line(x + 2, center_y - 2, x, center_y, 0xFFFFFF);
    gfx_draw_line(x + 2, center_y + 2, x, center_y, 0xFFFFFF);
    // Right arrow
    gfx_draw_line(x + 14, center_y - 2, x + 16, center_y, 0xFFFFFF);
    gfx_draw_line(x + 14, center_y + 2, x + 16, center_y, 0xFFFFFF);
}

/**
 * Render vertical resize cursor
 */
static void render_resize_v_cursor(uint32_t x, uint32_t y) {
    // Vertical double arrow
    uint32_t center_x = x + 8;
    gfx_draw_line(center_x, y, center_x, y + 16, 0xFFFFFF);
    // Top arrow
    gfx_draw_line(center_x - 2, y + 2, center_x, y, 0xFFFFFF);
    gfx_draw_line(center_x + 2, y + 2, center_x, y, 0xFFFFFF);
    // Bottom arrow
    gfx_draw_line(center_x - 2, y + 14, center_x, y + 16, 0xFFFFFF);
    gfx_draw_line(center_x + 2, y + 14, center_x, y + 16, 0xFFFFFF);
}

/**
 * Render crosshair cursor
 */
static void render_crosshair_cursor(uint32_t x, uint32_t y) {
    uint32_t center_x = x + 8;
    uint32_t center_y = y + 8;
    
    // Horizontal line
    gfx_draw_line(x, center_y, x + 16, center_y, 0xFFFFFF);
    // Vertical line
    gfx_draw_line(center_x, y, center_x, y + 16, 0xFFFFFF);
    // Center circle
    gfx_draw_circle(center_x, center_y, 2, 0xFFFFFF);
}

/**
 * Render diagonal resize cursor (top-left to bottom-right)
 */
static void render_resize_diag1_cursor(uint32_t x, uint32_t y) {
    // Main diagonal line
    gfx_draw_line(x, y, x + 16, y + 16, 0xFFFFFF);
    
    // Top-left arrow
    gfx_draw_line(x + 2, y, x, y + 2, 0xFFFFFF);
    gfx_draw_line(x, y + 2, x + 2, y + 4, 0xFFFFFF);
    
    // Bottom-right arrow
    gfx_draw_line(x + 14, y + 14, x + 16, y + 16, 0xFFFFFF);
    gfx_draw_line(x + 12, y + 14, x + 14, y + 16, 0xFFFFFF);
}

/**
 * Render diagonal resize cursor (top-right to bottom-left)
 */
static void render_resize_diag2_cursor(uint32_t x, uint32_t y) {
    // Main diagonal line
    gfx_draw_line(x + 16, y, x, y + 16, 0xFFFFFF);
    
    // Top-right arrow
    gfx_draw_line(x + 14, y, x + 16, y + 2, 0xFFFFFF);
    gfx_draw_line(x + 16, y + 2, x + 14, y + 4, 0xFFFFFF);
    
    // Bottom-left arrow
    gfx_draw_line(x + 2, y + 14, x, y + 16, 0xFFFFFF);
    gfx_draw_line(x, y + 14, x + 2, y + 16, 0xFFFFFF);
}

/**
 * Render animated wait cursor (rotating spinner)
 */
static void render_wait_cursor(uint32_t x, uint32_t y) {
    // Get current time for animation
    extern uint64_t time_get_uptime_ms(void);
    uint64_t uptime_ms = time_get_uptime_ms();
    
    uint32_t center_x = x + 8;
    uint32_t center_y = y + 8;
    
    // Draw rotating spinner (8 spokes)
    // Rotate based on time (one full rotation every 1000ms)
    uint32_t radius = 6;
    uint32_t base_angle = (uptime_ms / 10) % 360;  // Rotate every 10ms
    
    for (int i = 0; i < 8; i++) {
        uint32_t angle = (i * 45 + base_angle) % 360;
        double rad = (angle * M_PI) / 180.0;
        
        uint32_t end_x = center_x + (uint32_t)(radius * cos(rad));
        uint32_t end_y = center_y + (uint32_t)(radius * sin(rad));
        
        // Draw spoke from center to edge
        gfx_draw_line(center_x, center_y, end_x, end_y, 0xFFFFFF);
    }
}

/**
 * Render cursor at current position
 */
void cursor_render(void) {
    if (!g_cursor.visible || g_cursor.type == CURSOR_NONE) {
        return;
    }
    
    cursor_render_at(g_cursor.x - g_cursor.hot_x, g_cursor.y - g_cursor.hot_y);
}

/**
 * Render cursor at specific position
 */
void cursor_render_at(uint32_t x, uint32_t y) {
    if (!g_cursor.visible || g_cursor.type == CURSOR_NONE) {
        return;
    }
    
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return;
    }
    
    uint32_t fg_color = 0xFFFFFF;  // White
    uint32_t bg_color = 0x000000;  // Black (for XOR effect)
    
    switch (g_cursor.type) {
        case CURSOR_ARROW:
            render_bitmap_cursor(x, y, cursor_arrow_bitmap, cursor_arrow_mask, fg_color, bg_color);
            break;
        case CURSOR_TEXT:
            render_text_cursor(x, y);
            break;
        case CURSOR_HAND:
            render_bitmap_cursor(x, y, cursor_hand_bitmap, NULL, fg_color, bg_color);
            break;
        case CURSOR_RESIZE_H:
            render_resize_h_cursor(x, y);
            break;
        case CURSOR_RESIZE_V:
            render_resize_v_cursor(x, y);
            break;
        case CURSOR_RESIZE_DIAG1:
            // Diagonal resize cursor (top-left to bottom-right)
            render_resize_diag1_cursor(x, y);
            break;
        case CURSOR_RESIZE_DIAG2:
            // Diagonal resize cursor (top-right to bottom-left)
            render_resize_diag2_cursor(x, y);
            break;
        case CURSOR_WAIT:
            // Animated wait cursor (rotating spinner)
            render_wait_cursor(x, y);
            break;
        case CURSOR_CROSSHAIR:
            render_crosshair_cursor(x, y);
            break;
        case CURSOR_NONE:
            break;
    }
}

/**
 * Get cursor dimensions
 */
void cursor_get_size(uint32_t* width, uint32_t* height) {
    if (width) *width = 16;
    if (height) *height = 16;
}

/**
 * Get cursor hotspot
 */
void cursor_get_hotspot(uint32_t* hot_x, uint32_t* hot_y) {
    if (hot_x) *hot_x = g_cursor.hot_x;
    if (hot_y) *hot_y = g_cursor.hot_y;
}

