/**
 * @file graphics.c
 * @brief 2D Graphics library implementation
 */

#include "../include/graphics/graphics.h"
#include "../include/graphics/framebuffer.h"
#include "../include/graphics/font.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"
#include "../include/math.h"

// Clipping rectangle
static struct {
    uint32_t x, y, width, height;
    bool enabled;
} clip_rect = {0, 0, 0, 0, false};

// Double buffering
static void* back_buffer = NULL;
static bool double_buffer_enabled = false;

/**
 * Check if point is within clip rectangle
 */
static bool is_point_clipped(uint32_t x, uint32_t y) {
    if (!clip_rect.enabled) {
        return false;
    }
    return (x < clip_rect.x || x >= clip_rect.x + clip_rect.width ||
            y < clip_rect.y || y >= clip_rect.y + clip_rect.height);
}

/**
 * Draw a line using Bresenham's algorithm
 */
void gfx_draw_line(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color) {
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return;
    }
    
    int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    uint32_t x = x1;
    uint32_t y = y1;
    
    while (1) {
        if (!is_point_clipped(x, y)) {
            set_pixel_buffer(x, y, color);
        }
        
        if (x == x2 && y == y2) {
            break;
        }
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

/**
 * Draw rectangle outline
 */
void gfx_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    // Top edge
    gfx_draw_line(x, y, x + width - 1, y, color);
    // Bottom edge
    gfx_draw_line(x, y + height - 1, x + width - 1, y + height - 1, color);
    // Left edge
    gfx_draw_line(x, y, x, y + height - 1, color);
    // Right edge
    gfx_draw_line(x + width - 1, y, x + width - 1, y + height - 1, color);
}

/**
 * Fill rectangle
 */
void gfx_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return;
    }
    
    // Apply clipping
    if (clip_rect.enabled) {
        if (x < clip_rect.x) {
            width -= (clip_rect.x - x);
            x = clip_rect.x;
        }
        if (y < clip_rect.y) {
            height -= (clip_rect.y - y);
            y = clip_rect.y;
        }
        if (x + width > clip_rect.x + clip_rect.width) {
            width = clip_rect.x + clip_rect.width - x;
        }
        if (y + height > clip_rect.y + clip_rect.height) {
            height = clip_rect.y + clip_rect.height - y;
        }
    }
    
    framebuffer_fill_rect(x, y, width, height, color);
}

/**
 * Draw circle outline using midpoint algorithm
 */
void gfx_draw_circle(uint32_t x, uint32_t y, uint32_t radius, uint32_t color) {
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return;
    }
    
    int32_t cx = (int32_t)x;
    int32_t cy = (int32_t)y;
    int32_t r = (int32_t)radius;
    
    int32_t px = 0;
    int32_t py = r;
    int32_t d = 1 - r;
    
    // Helper function to draw 8 symmetric points
    static void draw_circle_points(int32_t cx, int32_t cy, int32_t px, int32_t py, uint32_t color) {
        if (!is_point_clipped(cx + px, cy + py)) set_pixel_buffer(cx + px, cy + py, color);
        if (!is_point_clipped(cx - px, cy + py)) set_pixel_buffer(cx - px, cy + py, color);
        if (!is_point_clipped(cx + px, cy - py)) set_pixel_buffer(cx + px, cy - py, color);
        if (!is_point_clipped(cx - px, cy - py)) set_pixel_buffer(cx - px, cy - py, color);
        if (!is_point_clipped(cx + py, cy + px)) set_pixel_buffer(cx + py, cy + px, color);
        if (!is_point_clipped(cx - py, cy + px)) set_pixel_buffer(cx - py, cy + px, color);
        if (!is_point_clipped(cx + py, cy - px)) set_pixel_buffer(cx + py, cy - px, color);
        if (!is_point_clipped(cx - py, cy - px)) set_pixel_buffer(cx - py, cy - px, color);
    }
    
    draw_circle_points(cx, cy, px, py, color);
    
    while (px < py) {
        if (d < 0) {
            d += 2 * px + 3;
        } else {
            d += 2 * (px - py) + 5;
            py--;
        }
        px++;
        draw_circle_points(cx, cy, px, py, color);
    }
}

/**
 * Fill circle
 */
void gfx_fill_circle(uint32_t x, uint32_t y, uint32_t radius, uint32_t color) {
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return;
    }
    
    int32_t cx = (int32_t)x;
    int32_t cy = (int32_t)y;
    int32_t r = (int32_t)radius;
    
    // Fill by drawing horizontal lines
    for (int32_t py = -r; py <= r; py++) {
        int32_t px_squared = r * r - py * py;
        if (px_squared < 0) continue;
        int32_t px = (int32_t)sqrt((double)px_squared);
        int32_t x1 = cx - px;
        int32_t x2 = cx + px;
        
        // Draw horizontal line
        for (int32_t px2 = x1; px2 <= x2; px2++) {
            if (!is_point_clipped(px2, cy + py)) {
                set_pixel_buffer(px2, cy + py, color);
            }
        }
    }
}

/**
 * Draw a single character (8x8 font)
 */
void gfx_draw_char(uint32_t x, uint32_t y, char c, uint32_t color, uint32_t bg_color) {
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return;
    }
    
    // Fill background
    if (bg_color != 0xFFFFFFFF) {  // 0xFFFFFFFF = transparent
        gfx_fill_rect(x, y, 8, 8, bg_color);
    }
    
    // Get font glyph
    const uint8_t* glyph = font_get_glyph(c);
    
    // Draw glyph bitmap
    for (uint32_t row = 0; row < 8; row++) {
        uint8_t byte = glyph[row];
        for (uint32_t col = 0; col < 8; col++) {
            if (byte & (1 << (7 - col))) {
                if (!is_point_clipped(x + col, y + row)) {
                    set_pixel_buffer(x + col, y + row, color);
                }
            }
        }
    }
}

/**
 * Draw a string
 */
void gfx_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t color, uint32_t bg_color) {
    if (!str) {
        return;
    }
    
    uint32_t cx = x;
    for (const char* p = str; *p; p++) {
        gfx_draw_char(cx, y, *p, color, bg_color);
        cx += 8;  // 8 pixels per character
    }
}

/**
 * Initialize double buffering
 */
void gfx_init_double_buffer(void) {
    framebuffer_t* fb = framebuffer_get();
    if (!fb || double_buffer_enabled) {
        return;
    }
    
    // Allocate back buffer
    size_t buffer_size = fb->height * fb->pitch;
    back_buffer = kmalloc(buffer_size);
    if (back_buffer) {
        double_buffer_enabled = true;
        // Clear back buffer
        memset(back_buffer, 0, buffer_size);
        kinfo("Double buffering enabled\n");
    } else {
        kwarn("Failed to allocate back buffer\n");
    }
}

/**
 * Swap front and back buffers
 */
void gfx_swap_buffers(void) {
    if (!double_buffer_enabled || !back_buffer) {
        return;
    }
    
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return;
    }
    
    // Copy back buffer to front buffer
    size_t buffer_size = fb->height * fb->pitch;
    memcpy(fb->base_address, back_buffer, buffer_size);
}

/**
 * Get back buffer pointer (for drawing operations)
 */
void* gfx_get_back_buffer(void) {
    return back_buffer;
}

/**
 * Set pixel in back buffer (if double buffering enabled)
 */
static void set_pixel_buffer(uint32_t x, uint32_t y, uint32_t color) {
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return;
    }
    
    if (double_buffer_enabled && back_buffer) {
        // Draw to back buffer
        uint32_t bytes_per_pixel = fb->bpp / 8;
        uint8_t* pixel_ptr = (uint8_t*)back_buffer;
        pixel_ptr += y * fb->pitch + x * bytes_per_pixel;
        
        if (bytes_per_pixel == 4) {
            *(uint32_t*)pixel_ptr = color;
        } else {
            // Fall back to front buffer
            framebuffer_set_pixel(x, y, color);
        }
    } else {
        // Draw directly to front buffer
        framebuffer_set_pixel(x, y, color);
    }
}

/**
 * Set clipping rectangle
 */
void gfx_set_clip_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return;
    }
    
    clip_rect.x = x;
    clip_rect.y = y;
    clip_rect.width = width;
    clip_rect.height = height;
    clip_rect.enabled = true;
}

/**
 * Reset clipping
 */
void gfx_reset_clip(void) {
    clip_rect.enabled = false;
}

/**
 * Blend pixel with alpha (simple alpha blending)
 */
void gfx_blend_pixel(uint32_t x, uint32_t y, uint32_t color, uint8_t alpha) {
    framebuffer_t* fb = framebuffer_get();
    if (!fb || is_point_clipped(x, y)) {
        return;
    }
    
    // Get current pixel color
    uint32_t bg_color = framebuffer_get_pixel(x, y);
    
    // Extract RGB components
    uint8_t fg_r = (color >> 16) & 0xFF;
    uint8_t fg_g = (color >> 8) & 0xFF;
    uint8_t fg_b = color & 0xFF;
    
    uint8_t bg_r = (bg_color >> 16) & 0xFF;
    uint8_t bg_g = (bg_color >> 8) & 0xFF;
    uint8_t bg_b = bg_color & 0xFF;
    
    // Alpha blend: result = (fg * alpha + bg * (255 - alpha)) / 255
    uint32_t alpha_inv = 255 - alpha;
    uint8_t r = (fg_r * alpha + bg_r * alpha_inv) / 255;
    uint8_t g = (fg_g * alpha + bg_g * alpha_inv) / 255;
    uint8_t b = (fg_b * alpha + bg_b * alpha_inv) / 255;
    
    uint32_t blended = RGB(r, g, b);
    set_pixel_buffer(x, y, blended);
}

/**
 * Draw rectangle with alpha transparency
 */
void gfx_draw_rect_alpha(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color, uint8_t alpha) {
    for (uint32_t py = y; py < y + height; py++) {
        for (uint32_t px = x; px < x + width; px++) {
            if (!is_point_clipped(px, py)) {
                gfx_blend_pixel(px, py, color, alpha);
            }
        }
    }
}

