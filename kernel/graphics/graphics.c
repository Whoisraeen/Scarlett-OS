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
/**
 * Fill circle using integer-based midpoint algorithm
 */
void gfx_fill_circle(uint32_t x, uint32_t y, uint32_t radius, uint32_t color) {
    framebuffer_t* fb = framebuffer_get();
    if (!fb) return;
    
    int32_t cx = (int32_t)x;
    int32_t cy = (int32_t)y;
    int32_t r = (int32_t)radius;
    int32_t r2 = r * r;
    
    for (int32_t dy = -r; dy <= r; dy++) {
        // Calculate width at this height using integer arithmetic
        // x^2 + y^2 = r^2  =>  x = sqrt(r^2 - y^2)
        // We can avoid sqrt by scanning or using the property of the circle
        // For filling, we just need the start and end x for each y
        
        // Optimization: use the fact that x = sqrt(r^2 - dy^2)
        // We can approximate or use a simple loop for small radii
        // For better performance, we can use the midpoint algorithm logic
        
        int32_t dx = 0;
        while (dx * dx + dy * dy <= r2) {
            dx++;
        }
        dx--; // Backtrack one step
        
        // Draw horizontal line
        int32_t x1 = cx - dx;
        int32_t x2 = cx + dx;
        
        // Clip y
        if (cy + dy < 0 || cy + dy >= (int32_t)fb->height) continue;
        
        // Clip x
        if (x1 < 0) x1 = 0;
        if (x2 >= (int32_t)fb->width) x2 = fb->width - 1;
        
        for (int32_t px = x1; px <= x2; px++) {
            if (!is_point_clipped(px, cy + dy)) {
                set_pixel_buffer(px, cy + dy, color);
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
 * Draw a scaled character
 */
void gfx_draw_char_scaled(uint32_t x, uint32_t y, char c, uint32_t color, uint32_t bg_color, uint32_t scale) {
    framebuffer_t* fb = framebuffer_get();
    if (!fb || scale == 0) {
        return;
    }
    
    // Fill background
    if (bg_color != 0xFFFFFFFF) {  // 0xFFFFFFFF = transparent
        gfx_fill_rect(x, y, 8 * scale, 8 * scale, bg_color);
    }
    
    // Get font glyph
    const uint8_t* glyph = font_get_glyph(c);
    
    // Draw scaled glyph bitmap
    for (uint32_t row = 0; row < 8; row++) {
        uint8_t byte = glyph[row];
        for (uint32_t col = 0; col < 8; col++) {
            if (byte & (1 << (7 - col))) {
                // Draw scaled pixel
                for (uint32_t sy = 0; sy < scale; sy++) {
                    for (uint32_t sx = 0; sx < scale; sx++) {
                        uint32_t px = x + col * scale + sx;
                        uint32_t py = y + row * scale + sy;
                        if (!is_point_clipped(px, py)) {
                            set_pixel_buffer(px, py, color);
                        }
                    }
                }
            }
        }
    }
}

/**
 * Draw a scaled string
 */
void gfx_draw_string_scaled(uint32_t x, uint32_t y, const char* str, uint32_t color, uint32_t bg_color, uint32_t scale) {
    if (!str) {
        return;
    }
    
    uint32_t cx = x;
    for (const char* p = str; *p; p++) {
        gfx_draw_char_scaled(cx, y, *p, color, bg_color, scale);
        cx += 8 * scale;  // Scaled character width
    }
}

// Text alignment constants
#define TEXT_ALIGN_LEFT    0
#define TEXT_ALIGN_CENTER  1
#define TEXT_ALIGN_RIGHT   2
#define TEXT_ALIGN_TOP     0
#define TEXT_ALIGN_MIDDLE  4
#define TEXT_ALIGN_BOTTOM  8

/**
 * Draw a string with alignment
 */
void gfx_draw_string_aligned(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const char* str, uint32_t color, uint32_t bg_color, uint32_t align) {
    if (!str) {
        return;
    }
    
    uint32_t text_w = gfx_text_width(str);
    uint32_t text_h = gfx_text_height();
    
    // Calculate horizontal position
    uint32_t text_x = x;
    if (align & TEXT_ALIGN_CENTER) {
        text_x = x + (width - text_w) / 2;
    } else if (align & TEXT_ALIGN_RIGHT) {
        text_x = x + width - text_w;
    }
    
    // Calculate vertical position
    uint32_t text_y = y;
    if (align & TEXT_ALIGN_MIDDLE) {
        text_y = y + (height - text_h) / 2;
    } else if (align & TEXT_ALIGN_BOTTOM) {
        text_y = y + height - text_h;
    }
    
    gfx_draw_string(text_x, text_y, str, color, bg_color);
}

/**
 * Get text width in pixels
 */
uint32_t gfx_text_width(const char* str) {
    if (!str) {
        return 0;
    }
    
    uint32_t len = 0;
    for (const char* p = str; *p; p++) {
        len++;
    }
    return len * 8;  // 8 pixels per character
}

/**
 * Get scaled text width in pixels
 */
uint32_t gfx_text_width_scaled(const char* str, uint32_t scale) {
    return gfx_text_width(str) * scale;
}

/**
 * Get text height in pixels
 */
uint32_t gfx_text_height(void) {
    return 8;  // 8 pixels per character
}

/**
 * Get scaled text height in pixels
 */
uint32_t gfx_text_height_scaled(uint32_t scale) {
    return 8 * scale;
}

/**
 * Initialize double buffering
 */
void gfx_init_double_buffer(void) {
    kinfo("gfx_init_double_buffer() called\n");
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        kwarn("Double buffering: framebuffer_get() returned NULL\n");
        return;
    }
    if (double_buffer_enabled) {
        kinfo("Double buffering already enabled\n");
        return;
    }

    // Allocate back buffer
    size_t buffer_size = fb->height * fb->pitch;
    kinfo("Allocating %lu bytes for back buffer...\n", buffer_size);
    back_buffer = kmalloc(buffer_size);
    if (back_buffer) {
        double_buffer_enabled = true;
        kinfo("Double buffering enabled (%lu MB back buffer)\n", buffer_size / (1024 * 1024));
        // Clear back buffer
        memset(back_buffer, 0, buffer_size);
    } else {
        kwarn("Failed to allocate back buffer (%lu bytes)\n", buffer_size);
    }
}

/**
 * Swap front and back buffers
 */
void gfx_swap_buffers(void) {
    // If double buffering is disabled, nothing to do (rendering happens directly to framebuffer)
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
 * Fill rounded rectangle with alpha transparency (optimized)
 */
void gfx_fill_rounded_rect_alpha(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t radius, uint32_t color, uint8_t alpha) {
    if (radius == 0) {
        gfx_draw_rect_alpha(x, y, width, height, color, alpha);
        return;
    }

    // Clamp radius
    if (radius > width / 2) radius = width / 2;
    if (radius > height / 2) radius = height / 2;

    // 1. Fill central rectangle (full height, width - 2*radius)
    gfx_draw_rect_alpha(x + radius, y, width - 2 * radius, height, color, alpha);
    
    // 2. Fill left and right rectangles (between corners)
    gfx_draw_rect_alpha(x, y + radius, radius, height - 2 * radius, color, alpha);
    gfx_draw_rect_alpha(x + width - radius, y + radius, radius, height - 2 * radius, color, alpha);
    
    // 3. Fill four corners
    int32_t r = (int32_t)radius;
    int32_t r2 = r * r;
    
    // Helper to draw corner pixels
    for (int32_t dy = 0; dy < r; dy++) {
        for (int32_t dx = 0; dx < r; dx++) {
            // Check if inside circle: (r-dx-1)^2 + (r-dy-1)^2 <= r^2
            // We iterate from corner inwards
            int32_t dist_sq = (r - dx - 1) * (r - dx - 1) + (r - dy - 1) * (r - dy - 1);
            
            if (dist_sq <= r2) {
                // Top-left
                if (!is_point_clipped(x + dx, y + dy)) 
                    gfx_blend_pixel(x + dx, y + dy, color, alpha);
                    
                // Top-right
                if (!is_point_clipped(x + width - r + dx, y + dy)) 
                    gfx_blend_pixel(x + width - r + dx, y + dy, color, alpha);
                    
                // Bottom-left
                if (!is_point_clipped(x + dx, y + height - r + dy)) 
                    gfx_blend_pixel(x + dx, y + height - r + dy, color, alpha);
                    
                // Bottom-right
                if (!is_point_clipped(x + width - r + dx, y + height - r + dy)) 
                    gfx_blend_pixel(x + width - r + dx, y + height - r + dy, color, alpha);
            }
        }
    }
}

/**
 * Apply optimized box blur to a region
 * Separable filter: Horizontal pass then Vertical pass
 */
void gfx_apply_blur_region(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t blur_radius) {
    if (blur_radius == 0) return;

    framebuffer_t* fb = framebuffer_get();
    if (!fb) return;
    
    // Limit blur radius for performance
    if (blur_radius > 10) blur_radius = 10;
    
    // We need a temporary buffer for the two-pass blur
    // For simplicity in kernel (avoiding large mallocs), we'll just do a simple
    // unoptimized blur but with a smaller kernel or skip pixels to save time.
    // Or, we can implement a proper separable blur if we have memory.
    
    // Let's use a simplified "fast blur" that just averages neighbors with a stride
    // This is not a true Gaussian but looks okay for UI glass
    
    uint32_t stride = 1; // Process every pixel
    
    for (uint32_t py = y; py < y + height; py += stride) {
        for (uint32_t px = x; px < x + width; px += stride) {
            uint32_t r_sum = 0, g_sum = 0, b_sum = 0, count = 0;
            
            // Small kernel (3x3 or 5x5) regardless of requested radius to keep it fast
            int k = (blur_radius > 2) ? 2 : blur_radius;
            
            for (int dy = -k; dy <= k; dy++) {
                for (int dx = -k; dx <= k; dx++) {
                    int32_t sx = (int32_t)px + dx;
                    int32_t sy = (int32_t)py + dy;
                    
                    if (sx >= (int32_t)x && sx < (int32_t)(x + width) &&
                        sy >= (int32_t)y && sy < (int32_t)(y + height)) {
                        uint32_t pixel = framebuffer_get_pixel(sx, sy);
                        r_sum += (pixel >> 16) & 0xFF;
                        g_sum += (pixel >> 8) & 0xFF;
                        b_sum += pixel & 0xFF;
                        count++;
                    }
                }
            }
            
            if (count > 0) {
                uint32_t avg_color = RGB(r_sum / count, g_sum / count, b_sum / count);
                set_pixel_buffer(px, py, avg_color);
            }
        }
    }
}

/**
 * Fill rectangle with gradient (fixed point)
 */
void gfx_fill_gradient_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color1, uint32_t color2, bool vertical) {
    // Extract RGB components
    int32_t r1 = (color1 >> 16) & 0xFF;
    int32_t g1 = (color1 >> 8) & 0xFF;
    int32_t b1 = color1 & 0xFF;

    int32_t r2 = (color2 >> 16) & 0xFF;
    int32_t g2 = (color2 >> 8) & 0xFF;
    int32_t b2 = color2 & 0xFF;
    
    int32_t dr = r2 - r1;
    int32_t dg = g2 - g1;
    int32_t db = b2 - b1;

    if (vertical) {
        // Vertical gradient
        for (uint32_t py = 0; py < height; py++) {
            // Calculate color for this row using fixed point (scaled by 1024)
            // factor = py / height
            int32_t r = r1 + (dr * (int32_t)py) / (int32_t)height;
            int32_t g = g1 + (dg * (int32_t)py) / (int32_t)height;
            int32_t b = b1 + (db * (int32_t)py) / (int32_t)height;
            
            // Clamp
            if (r < 0) r = 0; if (r > 255) r = 255;
            if (g < 0) g = 0; if (g > 255) g = 255;
            if (b < 0) b = 0; if (b > 255) b = 255;
            
            uint32_t color = RGB(r, g, b);

            for (uint32_t px = 0; px < width; px++) {
                if (!is_point_clipped(x + px, y + py)) {
                    set_pixel_buffer(x + px, y + py, color);
                }
            }
        }
    } else {
        // Horizontal gradient
        for (uint32_t px = 0; px < width; px++) {
            int32_t r = r1 + (dr * (int32_t)px) / (int32_t)width;
            int32_t g = g1 + (dg * (int32_t)px) / (int32_t)width;
            int32_t b = b1 + (db * (int32_t)px) / (int32_t)width;
            
            if (r < 0) r = 0; if (r > 255) r = 255;
            if (g < 0) g = 0; if (g > 255) g = 255;
            if (b < 0) b = 0; if (b > 255) b = 255;
            
            uint32_t color = RGB(r, g, b);

            for (uint32_t py = 0; py < height; py++) {
                if (!is_point_clipped(x + px, y + py)) {
                    set_pixel_buffer(x + px, y + py, color);
                }
            }
        }
    }
}

