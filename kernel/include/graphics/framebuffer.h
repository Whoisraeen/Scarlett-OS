/**
 * @file framebuffer.h
 * @brief Framebuffer driver interface
 */

#ifndef KERNEL_GRAPHICS_FRAMEBUFFER_H
#define KERNEL_GRAPHICS_FRAMEBUFFER_H

#include "../types.h"
#include "../errors.h"
#include "../../../bootloader/common/boot_info.h"

// Framebuffer structure
typedef struct {
    void* base_address;          // Base address of framebuffer
    uint32_t width;             // Width in pixels
    uint32_t height;            // Height in pixels
    uint32_t pitch;             // Bytes per scanline
    uint32_t bpp;               // Bits per pixel
    uint32_t red_mask;          // Red color mask
    uint32_t green_mask;        // Green color mask
    uint32_t blue_mask;         // Blue color mask
    uint32_t reserved_mask;     // Reserved/alpha mask
    bool initialized;           // Is framebuffer initialized?
} framebuffer_t;

// Color manipulation macros
#define RGB(r, g, b) ((uint32_t)((r) << 16) | ((g) << 8) | (b))
#define RGBA(r, g, b, a) ((uint32_t)((a) << 24) | ((r) << 16) | ((g) << 8) | (b))

// Common colors
#define COLOR_BLACK     RGB(0, 0, 0)
#define COLOR_WHITE     RGB(255, 255, 255)
#define COLOR_RED       RGB(255, 0, 0)
#define COLOR_GREEN     RGB(0, 255, 0)
#define COLOR_BLUE      RGB(0, 0, 255)
#define COLOR_CYAN      RGB(0, 255, 255)
#define COLOR_MAGENTA   RGB(255, 0, 255)
#define COLOR_YELLOW    RGB(255, 255, 0)

// Framebuffer functions
error_code_t framebuffer_init(framebuffer_info_t* boot_fb_info);
framebuffer_t* framebuffer_get(void);
void framebuffer_set_pixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t framebuffer_get_pixel(uint32_t x, uint32_t y);
void framebuffer_clear(uint32_t color);
void framebuffer_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);

#endif // KERNEL_GRAPHICS_FRAMEBUFFER_H

