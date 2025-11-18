/**
 * @file graphics.h
 * @brief 2D Graphics library interface
 */

#ifndef KERNEL_GRAPHICS_GRAPHICS_H
#define KERNEL_GRAPHICS_GRAPHICS_H

#include "../types.h"
#include "framebuffer.h"

// Drawing functions
void gfx_draw_line(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color);
void gfx_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void gfx_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void gfx_draw_circle(uint32_t x, uint32_t y, uint32_t radius, uint32_t color);
void gfx_fill_circle(uint32_t x, uint32_t y, uint32_t radius, uint32_t color);

// Text rendering (basic)
void gfx_draw_char(uint32_t x, uint32_t y, char c, uint32_t color, uint32_t bg_color);
void gfx_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t color, uint32_t bg_color);

// Double buffering
void gfx_init_double_buffer(void);
void gfx_swap_buffers(void);
void* gfx_get_back_buffer(void);

// Clipping
void gfx_set_clip_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void gfx_reset_clip(void);

// Alpha blending (basic)
void gfx_blend_pixel(uint32_t x, uint32_t y, uint32_t color, uint8_t alpha);
void gfx_draw_rect_alpha(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color, uint8_t alpha);

#endif // KERNEL_GRAPHICS_GRAPHICS_H

