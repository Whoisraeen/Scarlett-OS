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

// Forward declarations
static void set_pixel_buffer(uint32_t x, uint32_t y, uint32_t color);

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
 * Helper function to draw 8 symmetric points of a circle
 */
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

/**
 * Fill rounded rectangle (solid color)
 */
void gfx_fill_rounded_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t radius, uint32_t color) {
    if (radius == 0) {
        gfx_fill_rect(x, y, width, height, color);
        return;
    }

    // Clamp radius to half of smallest dimension
    if (radius > width / 2) radius = width / 2;
    if (radius > height / 2) radius = height / 2;

    // Fill center rectangle (full width, minus rounded corners)
    gfx_fill_rect(x, y + radius, width, height - 2 * radius, color);

    // Fill top and bottom rectangles (between corners)
    gfx_fill_rect(x + radius, y, width - 2 * radius, radius, color);
    gfx_fill_rect(x + radius, y + height - radius, width - 2 * radius, radius, color);

    // Fill four corner circles
    gfx_fill_circle(x + radius, y + radius, radius, color);  // Top-left
    gfx_fill_circle(x + width - radius - 1, y + radius, radius, color);  // Top-right
    gfx_fill_circle(x + radius, y + height - radius - 1, radius, color);  // Bottom-left
    gfx_fill_circle(x + width - radius - 1, y + height - radius - 1, radius, color);  // Bottom-right
}

/**
 * Fill rounded rectangle with alpha transparency (glassmorphism)
 */
void gfx_fill_rounded_rect_alpha(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t radius, uint32_t color, uint8_t alpha) {
    if (radius == 0) {
        gfx_draw_rect_alpha(x, y, width, height, color, alpha);
        return;
    }

    // Clamp radius
    if (radius > width / 2) radius = width / 2;
    if (radius > height / 2) radius = height / 2;

    // For each pixel, check if it's within the rounded rectangle bounds
    int32_t r2 = (int32_t)(radius * radius);

    for (uint32_t py = 0; py < height; py++) {
        for (uint32_t px = 0; px < width; px++) {
            uint32_t abs_x = x + px;
            uint32_t abs_y = y + py;

            bool in_shape = true;

            // Check if pixel is in a corner region
            if (px < radius && py < radius) {
                // Top-left corner
                int32_t dx = (int32_t)radius - (int32_t)px;
                int32_t dy = (int32_t)radius - (int32_t)py;
                if (dx * dx + dy * dy > r2) in_shape = false;
            } else if (px >= width - radius && py < radius) {
                // Top-right corner
                int32_t dx = (int32_t)px - (int32_t)(width - radius - 1);
                int32_t dy = (int32_t)radius - (int32_t)py;
                if (dx * dx + dy * dy > r2) in_shape = false;
            } else if (px < radius && py >= height - radius) {
                // Bottom-left corner
                int32_t dx = (int32_t)radius - (int32_t)px;
                int32_t dy = (int32_t)py - (int32_t)(height - radius - 1);
                if (dx * dx + dy * dy > r2) in_shape = false;
            } else if (px >= width - radius && py >= height - radius) {
                // Bottom-right corner
                int32_t dx = (int32_t)px - (int32_t)(width - radius - 1);
                int32_t dy = (int32_t)py - (int32_t)(height - radius - 1);
                if (dx * dx + dy * dy > r2) in_shape = false;
            }

            if (in_shape && !is_point_clipped(abs_x, abs_y)) {
                gfx_blend_pixel(abs_x, abs_y, color, alpha);
            }
        }
    }
}

/**
 * Draw rounded rectangle outline
 */
void gfx_draw_rounded_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t radius, uint32_t color) {
    if (radius == 0) {
        gfx_draw_rect(x, y, width, height, color);
        return;
    }

    // Clamp radius
    if (radius > width / 2) radius = width / 2;
    if (radius > height / 2) radius = height / 2;

    // Draw straight edges
    gfx_draw_line(x + radius, y, x + width - radius - 1, y, color);  // Top
    gfx_draw_line(x + radius, y + height - 1, x + width - radius - 1, y + height - 1, color);  // Bottom
    gfx_draw_line(x, y + radius, x, y + height - radius - 1, color);  // Left
    gfx_draw_line(x + width - 1, y + radius, x + width - 1, y + height - radius - 1, color);  // Right

    // Draw corner arcs
    gfx_draw_circle(x + radius, y + radius, radius, color);  // Top-left
    gfx_draw_circle(x + width - radius - 1, y + radius, radius, color);  // Top-right
    gfx_draw_circle(x + radius, y + height - radius - 1, radius, color);  // Bottom-left
    gfx_draw_circle(x + width - radius - 1, y + height - radius - 1, radius, color);  // Bottom-right
}

/**
 * Draw soft shadow around rounded rectangle (glassmorphism depth effect)
 */
void gfx_draw_shadow(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t radius, uint8_t intensity) {
    // Simple shadow: draw multiple alpha layers offset slightly
    uint32_t shadow_color = RGB(0, 0, 0);
    uint32_t shadow_offset = 4;
    uint32_t shadow_layers = 3;

    for (uint32_t i = 0; i < shadow_layers; i++) {
        uint8_t alpha = (intensity * (shadow_layers - i)) / (shadow_layers * 2);
        gfx_fill_rounded_rect_alpha(x + shadow_offset + i, y + shadow_offset + i,
                                     width, height, radius, shadow_color, alpha);
    }
}

/**
 * Apply simple box blur to a region (glassmorphism frosted glass effect)
 */
void gfx_apply_blur_region(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t blur_radius) {
    if (blur_radius == 0) return;

    framebuffer_t* fb = framebuffer_get();
    if (!fb) return;

    // Simple box blur - average pixels in a radius
    // Note: This is expensive, use sparingly or pre-compute
    for (uint32_t py = y; py < y + height; py++) {
        for (uint32_t px = x; px < x + width; px++) {
            uint32_t r_sum = 0, g_sum = 0, b_sum = 0, count = 0;

            // Average surrounding pixels
            for (int32_t dy = -(int32_t)blur_radius; dy <= (int32_t)blur_radius; dy++) {
                for (int32_t dx = -(int32_t)blur_radius; dx <= (int32_t)blur_radius; dx++) {
                    int32_t sample_x = (int32_t)px + dx;
                    int32_t sample_y = (int32_t)py + dy;

                    if (sample_x >= (int32_t)x && sample_x < (int32_t)(x + width) &&
                        sample_y >= (int32_t)y && sample_y < (int32_t)(y + height)) {
                        uint32_t pixel = framebuffer_get_pixel(sample_x, sample_y);
                        r_sum += (pixel >> 16) & 0xFF;
                        g_sum += (pixel >> 8) & 0xFF;
                        b_sum += pixel & 0xFF;
                        count++;
                    }
                }
            }

            if (count > 0) {
                uint8_t r_avg = r_sum / count;
                uint8_t g_avg = g_sum / count;
                uint8_t b_avg = b_sum / count;
                set_pixel_buffer(px, py, RGB(r_avg, g_avg, b_avg));
            }
        }
    }
}

/**
 * Fill rectangle with gradient (for modern backgrounds)
 */
void gfx_fill_gradient_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color1, uint32_t color2, bool vertical) {
    // Extract RGB components of both colors
    uint8_t r1 = (color1 >> 16) & 0xFF;
    uint8_t g1 = (color1 >> 8) & 0xFF;
    uint8_t b1 = color1 & 0xFF;

    uint8_t r2 = (color2 >> 16) & 0xFF;
    uint8_t g2 = (color2 >> 8) & 0xFF;
    uint8_t b2 = color2 & 0xFF;

    if (vertical) {
        // Vertical gradient (top to bottom)
        for (uint32_t py = 0; py < height; py++) {
            float t = (float)py / (float)height;
            uint8_t r = (uint8_t)(r1 + (r2 - r1) * t);
            uint8_t g = (uint8_t)(g1 + (g2 - g1) * t);
            uint8_t b = (uint8_t)(b1 + (b2 - b1) * t);
            uint32_t color = RGB(r, g, b);

            for (uint32_t px = 0; px < width; px++) {
                if (!is_point_clipped(x + px, y + py)) {
                    set_pixel_buffer(x + px, y + py, color);
                }
            }
        }
    } else {
        // Horizontal gradient (left to right)
        for (uint32_t px = 0; px < width; px++) {
            float t = (float)px / (float)width;
            uint8_t r = (uint8_t)(r1 + (r2 - r1) * t);
            uint8_t g = (uint8_t)(g1 + (g2 - g1) * t);
            uint8_t b = (uint8_t)(b1 + (b2 - b1) * t);
            uint32_t color = RGB(r, g, b);

            for (uint32_t py = 0; py < height; py++) {
                if (!is_point_clipped(x + px, y + py)) {
                    set_pixel_buffer(x + px, y + py, color);
                }
            }
        }
    }
}

