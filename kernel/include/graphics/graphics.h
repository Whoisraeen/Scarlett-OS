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

// Text rendering
void gfx_draw_char(uint32_t x, uint32_t y, char c, uint32_t color, uint32_t bg_color);
void gfx_draw_char_scaled(uint32_t x, uint32_t y, char c, uint32_t color, uint32_t bg_color, uint32_t scale);
void gfx_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t color, uint32_t bg_color);
void gfx_draw_string_scaled(uint32_t x, uint32_t y, const char* str, uint32_t color, uint32_t bg_color, uint32_t scale);
void gfx_draw_string_aligned(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const char* str, uint32_t color, uint32_t bg_color, uint32_t align);

// Text measurement
uint32_t gfx_text_width(const char* str);
uint32_t gfx_text_width_scaled(const char* str, uint32_t scale);
uint32_t gfx_text_height(void);
uint32_t gfx_text_height_scaled(uint32_t scale);

// Text alignment constants
#define TEXT_ALIGN_LEFT    0
#define TEXT_ALIGN_CENTER  1
#define TEXT_ALIGN_RIGHT   2
#define TEXT_ALIGN_TOP     0
#define TEXT_ALIGN_MIDDLE  4
#define TEXT_ALIGN_BOTTOM  8

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

// Glassmorphism effects
void gfx_fill_rounded_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t radius, uint32_t color);
void gfx_fill_rounded_rect_alpha(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t radius, uint32_t color, uint8_t alpha);
void gfx_draw_rounded_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t radius, uint32_t color);
void gfx_draw_shadow(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t radius, uint8_t intensity);
void gfx_apply_blur_region(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t blur_radius);
void gfx_fill_gradient_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color1, uint32_t color2, bool vertical);

#endif // KERNEL_GRAPHICS_GRAPHICS_H

