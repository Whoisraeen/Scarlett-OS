/**
 * @file accel.h
 * @brief 2D Graphics Acceleration API
 */

#ifndef KERNEL_GRAPHICS_ACCEL_H
#define KERNEL_GRAPHICS_ACCEL_H

#include "../types.h"
#include "../errors.h"

// Acceleration capabilities
typedef struct {
    bool blit_supported;          // Blit operations
    bool fill_supported;          // Rectangle fill
    bool alpha_blend_supported;   // Alpha blending
    bool rotate_supported;        // Rotation
    bool scale_supported;         // Scaling
} gfx_accel_caps_t;

// Blit operation flags
#define GFX_BLIT_NONE    0
#define GFX_BLIT_ALPHA   1
#define GFX_BLIT_COLORKEY 2

// Acceleration functions
error_code_t gfx_accel_init(void);
gfx_accel_caps_t* gfx_accel_get_caps(void);
error_code_t gfx_accel_blit(uint32_t src_x, uint32_t src_y, uint32_t width, uint32_t height,
                            uint32_t dest_x, uint32_t dest_y, void* src_fb, void* dest_fb,
                            uint32_t flags);
error_code_t gfx_accel_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                uint32_t color, void* fb);
error_code_t gfx_accel_copy_rect(uint32_t src_x, uint32_t src_y, uint32_t width, uint32_t height,
                                uint32_t dest_x, uint32_t dest_y, void* fb);
error_code_t gfx_accel_alpha_blend(uint32_t src_x, uint32_t src_y, uint32_t width, uint32_t height,
                                   uint32_t dest_x, uint32_t dest_y, uint8_t alpha,
                                   void* src_fb, void* dest_fb);

#endif // KERNEL_GRAPHICS_ACCEL_H

