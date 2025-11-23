#ifndef _SCARLETTOS_GRAPHICS_H
#define _SCARLETTOS_GRAPHICS_H

#include <scarlettos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
    uint32_t format;
    void *buffer;
} sc_framebuffer_t;

// Graphics context
typedef uint32_t sc_gfx_ctx_t;

// Basic graphics operations
int sc_gfx_init(void);
int sc_gfx_get_framebuffer(sc_framebuffer_t *fb);
int sc_gfx_swap_buffers(void);
int sc_gfx_draw_rect(int x, int y, int w, int h, uint32_t color);
int sc_gfx_draw_pixel(int x, int y, uint32_t color);
int sc_gfx_blit(const void *src, int x, int y, int w, int h);

#ifdef __cplusplus
}
#endif

#endif // _SCARLETTOS_GRAPHICS_H
