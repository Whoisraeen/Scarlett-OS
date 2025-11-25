/**
 * @file ugal.c
 * @brief Universal GPU Abstraction Layer Implementation
 */

#include "ugal.h"
#include <string.h>
#include <stdlib.h> // For malloc, free
#include <stdio.h> // For debugging printfs, can be removed

// Maximum number of GPU devices
#define MAX_GPU_DEVICES 8

// UGAL device structure
struct ugal_device {
    ugal_device_info_t info;
    void* driver_data;  // Vendor-specific driver data

    // Function pointers for vendor-specific operations (mocked for now)
    void* (*create_buffer)(void* driver_data, uint64_t size, uint32_t usage);
    void (*destroy_buffer)(void* driver_data, void* buffer);
    void* (*map_buffer)(void* driver_data, void* buffer);
    void (*unmap_buffer)(void* driver_data, void* buffer);
    void (*update_buffer)(void* driver_data, void* buffer, const void* data, uint64_t offset, uint64_t size);

    void* (*create_texture)(void* driver_data, uint32_t width, uint32_t height, ugal_format_t format);
    void (*destroy_texture)(void* driver_data, void* texture);
    void (*update_texture)(void* driver_data, void* texture, const void* data, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

    void* (*create_framebuffer)(void* driver_data, uint32_t width, uint32_t height);
    void (*destroy_framebuffer)(void* driver_data, void* framebuffer);

    void (*clear)(void* driver_data, void* framebuffer, uint32_t color);
    void (*fill_rect)(void* driver_data, void* framebuffer, int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color);
    void (*draw_line)(void* driver_data, void* framebuffer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
    void (*blit)(void* driver_data, void* src, void* dst, int32_t sx, int32_t sy, int32_t dx, int32_t dy, uint32_t width, uint32_t height);
    
    void (*present)(void* driver_data, void* framebuffer);
    void (*set_vsync)(void* driver_data, bool enable);
};

// UGAL buffer structure
struct ugal_buffer {
    ugal_device_t* device;
    void* driver_buffer;
    uint64_t size;
    uint32_t usage;
};

// UGAL texture structure
struct ugal_texture {
    ugal_device_t* device;
    void* driver_texture;
    uint32_t width;
    uint32_t height;
    ugal_format_t format;
    void* data;  // Software fallback: pixel data (RGBA8 format)
};

// UGAL framebuffer structure
struct ugal_framebuffer {
    ugal_device_t* device;
    void* driver_framebuffer;
    ugal_texture_t* color_texture;
    ugal_texture_t* depth_texture;
    uint32_t width;
    uint32_t height;
};

// UGAL pipeline structure (simplified)
struct ugal_pipeline {
    ugal_device_t* device;
    void* driver_pipeline;
    char* vertex_shader;
    char* fragment_shader;
};

// UGAL command buffer structure (simplified)
struct ugal_command_buffer {
    ugal_device_t* device;
    void* driver_cmd;
    bool recording;
};

// Global device list
static ugal_device_t* g_devices[MAX_GPU_DEVICES] = {NULL};
static uint32_t g_device_count = 0;

// --- Mock Vendor-Specific Driver Hooks (for illustration) ---
static void* mock_create_buffer(void* driver_data, uint64_t size, uint32_t usage) {
    (void)driver_data; (void)size; (void)usage;
    // In a real driver, this would allocate GPU memory
    return malloc(size); // Software buffer
}
static void mock_destroy_buffer(void* driver_data, void* buffer) {
    (void)driver_data;
    free(buffer);
}
static void* mock_map_buffer(void* driver_data, void* buffer) {
    (void)driver_data;
    return buffer; // Software buffer is directly accessible
}
static void mock_unmap_buffer(void* driver_data, void* buffer) { (void)driver_data; (void)buffer; }
static void mock_update_buffer(void* driver_data, void* buffer, const void* data, uint64_t offset, uint64_t size) {
    (void)driver_data;
    memcpy((uint8_t*)buffer + offset, data, size);
}

static void* mock_create_texture(void* driver_data, uint32_t width, uint32_t height, ugal_format_t format) {
    (void)driver_data; (void)width; (void)height; (void)format;
    // In a real driver, this allocates GPU texture memory
    return NULL; // Software fallback handles pixel data
}
static void mock_destroy_texture(void* driver_data, void* texture) { (void)driver_data; free(texture); }
static void mock_update_texture(void* driver_data, void* texture, const void* data, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    (void)driver_data; (void)texture; (void)data; (void)x; (void)y; (void)width; (void)height;
    // Data is copied to software texture. Driver would upload to GPU.
}

static void* mock_create_framebuffer(void* driver_data, uint32_t width, uint32_t height) {
    (void)driver_data; (void)width; (void)height;
    // Real driver would create GPU framebuffer objects
    return NULL; // Software fallback for color_texture is sufficient
}
static void mock_destroy_framebuffer(void* driver_data, void* framebuffer) { (void)driver_data; free(framebuffer); }

static void mock_clear(void* driver_data, void* framebuffer, uint32_t color) { (void)driver_data; (void)framebuffer; (void)color; }
static void mock_fill_rect(void* driver_data, void* framebuffer, int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color) { (void)driver_data; (void)framebuffer; (void)x; (void)y; (void)width; (void)height; (void)color; }
static void mock_draw_line(void* driver_data, void* framebuffer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color) { (void)driver_data; (void)framebuffer; (void)x1; (void)y1; (void)x2; (void)y2; (void)color; }
static void mock_blit(void* driver_data, void* src, void* dst, int32_t sx, int32_t sy, int32_t dx, int32_t dy, uint32_t width, uint32_t height) { (void)driver_data; (void)src; (void)dst; (void)sx; (void)sy; (void)dx; (void)dy; (void)width; (void)height; }
static void mock_present(void* driver_data, void* framebuffer) { (void)driver_data; (void)framebuffer; }
static void mock_set_vsync(void* driver_data, bool enable) { (void)driver_data; (void)enable; }
// --- End Mock Hooks ---

// Enumerate available GPU devices
uint32_t ugal_enumerate_devices(ugal_device_info_t* devices, uint32_t max_devices) {
    // This would typically query PCI for GPU devices
    // For now, return a dummy VirtIO GPU device
    if (max_devices > 0 && devices != NULL) {
        devices[0].vendor = UGAL_VENDOR_VIRTIO;
        devices[0].device_id = 0x1050;
        strcpy(devices[0].name, "VirtIO GPU");
        devices[0].vram_size = 256 * 1024 * 1024; // 256 MB
        devices[0].capabilities = UGAL_CAP_2D_ACCEL | UGAL_CAP_DISPLAY_OUTPUT;
        devices[0].max_texture_size = 4096;
        devices[0].max_render_targets = 8;

        return 1;
    }

    return 0;
}

// Create UGAL device
ugal_device_t* ugal_create_device(uint32_t device_index) {
    if (device_index >= MAX_GPU_DEVICES) {
        return NULL;
    }

    ugal_device_t* device = (ugal_device_t*)malloc(sizeof(ugal_device_t));
    if (!device) {
        return NULL;
    }

    memset(device, 0, sizeof(ugal_device_t));

    // Get device info
    ugal_enumerate_devices(&device->info, 1);

    // Initialize vendor-specific driver (mocking for now)
    device->create_buffer = mock_create_buffer;
    device->destroy_buffer = mock_destroy_buffer;
    device->map_buffer = mock_map_buffer;
    device->unmap_buffer = mock_unmap_buffer;
    device->update_buffer = mock_update_buffer;

    device->create_texture = mock_create_texture;
    device->destroy_texture = mock_destroy_texture;
    device->update_texture = mock_update_texture;

    device->create_framebuffer = mock_create_framebuffer;
    device->destroy_framebuffer = mock_destroy_framebuffer;

    device->clear = mock_clear;
    device->fill_rect = mock_fill_rect;
    device->draw_line = mock_draw_line;
    device->blit = mock_blit;

    device->present = mock_present;
    device->set_vsync = mock_set_vsync;
    
    // Vendor-specific driver data would be allocated/initialized here
    device->driver_data = NULL; // No specific data for mock

    g_devices[g_device_count++] = device;

    return device;
}

// Destroy UGAL device
void ugal_destroy_device(ugal_device_t* device) {
    if (!device) return;

    // Cleanup vendor-specific driver (mocking for now)
    if (device->driver_data) {
        free(device->driver_data);
    }

    free(device);
}

// Get device information
void ugal_get_device_info(ugal_device_t* device, ugal_device_info_t* info) {
    if (!device || !info) return;

    memcpy(info, &device->info, sizeof(ugal_device_info_t));
}

// Create buffer
ugal_buffer_t* ugal_create_buffer(ugal_device_t* device, uint64_t size, uint32_t usage) {
    if (!device) return NULL;

    ugal_buffer_t* buffer = (ugal_buffer_t*)malloc(sizeof(ugal_buffer_t));
    if (!buffer) return NULL;

    buffer->device = device;
    buffer->size = size;
    buffer->usage = usage;

    // Call vendor-specific buffer creation
    if (device->create_buffer) {
        buffer->driver_buffer = device->create_buffer(device->driver_data, size, usage);
    } else {
        buffer->driver_buffer = malloc(size); // Software fallback
    }

    return buffer;
}

// Destroy buffer
void ugal_destroy_buffer(ugal_buffer_t* buffer) {
    if (!buffer) return;

    // Call vendor-specific buffer destruction
    if (buffer->device->destroy_buffer && buffer->driver_buffer) {
        buffer->device->destroy_buffer(buffer->device->driver_data, buffer->driver_buffer);
    } else if (buffer->driver_buffer) {
        free(buffer->driver_buffer); // Software fallback
    }

    free(buffer);
}

// Map buffer
void* ugal_map_buffer(ugal_buffer_t* buffer) {
    if (!buffer || !buffer->device) return NULL;

    // Call vendor-specific map
    if (buffer->device->map_buffer && buffer->driver_buffer) {
        return buffer->device->map_buffer(buffer->device->driver_data, buffer->driver_buffer);
    }
    return buffer->driver_buffer; // Software fallback is direct
}

// Unmap buffer
void ugal_unmap_buffer(ugal_buffer_t* buffer) {
    if (!buffer || !buffer->device) return;

    // Call vendor-specific unmap
    if (buffer->device->unmap_buffer && buffer->driver_buffer) {
        buffer->device->unmap_buffer(buffer->device->driver_data, buffer->driver_buffer);
    }
}

// Update buffer
void ugal_update_buffer(ugal_buffer_t* buffer, const void* data, uint64_t offset, uint64_t size) {
    if (!buffer || !data) return;

    if (buffer->device->update_buffer && buffer->driver_buffer) {
        buffer->device->update_buffer(buffer->device->driver_data, buffer->driver_buffer, data, offset, size);
    } else {
        // Software fallback
        if (buffer->driver_buffer) {
            memcpy((uint8_t*)buffer->driver_buffer + offset, data, size);
        }
    }
}

// Create texture
ugal_texture_t* ugal_create_texture(ugal_device_t* device, uint32_t width, uint32_t height, ugal_format_t format) {
    if (!device) return NULL;

    ugal_texture_t* texture = (ugal_texture_t*)malloc(sizeof(ugal_texture_t));
    if (!texture) return NULL;

    texture->device = device;
    texture->width = width;
    texture->height = height;
    texture->format = format;
    texture->driver_texture = NULL;

    // Allocate software fallback pixel buffer (RGBA8)
    size_t pixel_size = 4; // RGBA8 = 4 bytes per pixel
    texture->data = malloc(width * height * pixel_size);
    if (!texture->data) {
        free(texture);
        return NULL;
    }
    memset(texture->data, 0, width * height * pixel_size);

    // Call vendor-specific texture creation
    if (device->create_texture) {
        texture->driver_texture = device->create_texture(device->driver_data, width, height, format);
    }

    return texture;
}

// Destroy texture
void ugal_destroy_texture(ugal_texture_t* texture) {
    if (!texture) return;

    // Call vendor-specific texture destruction
    if (texture->device->destroy_texture && texture->driver_texture) {
        texture->device->destroy_texture(texture->device->driver_data, texture->driver_texture);
    }

    // Free software fallback pixel buffer
    if (texture->data) {
        free(texture->data);
        texture->data = NULL;
    }

    free(texture);
}

// Update texture
void ugal_update_texture(ugal_texture_t* texture, const void* data, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (!texture || !data) return;

    // Validate bounds
    if (x + width > texture->width || y + height > texture->height) {
        return;
    }

    // Software fallback: copy pixel data
    if (texture->data) {
        uint32_t* dst = (uint32_t*)texture->data;
        const uint32_t* src = (const uint32_t*)data;
        
        // Copy row by row
        for (uint32_t row = 0; row < height; row++) {
            uint32_t dst_y = y + row;
            uint32_t src_offset = row * width;
            uint32_t dst_offset = dst_y * texture->width + x;
            memcpy(&dst[dst_offset], &src[src_offset], width * sizeof(uint32_t));
        }
    }

    // Call vendor-specific texture update
    if (texture->device->update_texture && texture->driver_texture) {
        texture->device->update_texture(texture->device->driver_data, texture->driver_texture, data, x, y, width, height);
    }
}

// Create framebuffer
ugal_framebuffer_t* ugal_create_framebuffer(ugal_device_t* device, uint32_t width, uint32_t height) {
    if (!device) return NULL;

    ugal_framebuffer_t* framebuffer = (ugal_framebuffer_t*)malloc(sizeof(ugal_framebuffer_t));
    if (!framebuffer) return NULL;

    framebuffer->device = device;
    framebuffer->width = width;
    framebuffer->height = height;
    framebuffer->color_texture = NULL;
    framebuffer->depth_texture = NULL;

    // Call vendor-specific framebuffer creation
    if (device->create_framebuffer) {
        framebuffer->driver_framebuffer = device->create_framebuffer(device->driver_data, width, height);
    }

    return framebuffer;
}

// Destroy framebuffer
void ugal_destroy_framebuffer(ugal_framebuffer_t* framebuffer) {
    if (!framebuffer) return;

    // Call vendor-specific framebuffer destruction
    if (framebuffer->device->destroy_framebuffer && framebuffer->driver_framebuffer) {
        framebuffer->device->destroy_framebuffer(framebuffer->device->driver_data, framebuffer->driver_framebuffer);
    }

    free(framebuffer);
}

// Attach color texture to framebuffer
void ugal_attach_color_texture(ugal_framebuffer_t* framebuffer, ugal_texture_t* texture) {
    if (!framebuffer || !texture) return;
    framebuffer->color_texture = texture;
}

// Attach depth texture to framebuffer
void ugal_attach_depth_texture(ugal_framebuffer_t* framebuffer, ugal_texture_t* texture) {
    if (!framebuffer || !texture) return;
    framebuffer->depth_texture = texture;
}


// 2D acceleration - Clear
void ugal_clear(ugal_device_t* device, ugal_framebuffer_t* framebuffer, uint32_t color) {
    if (!device || !framebuffer) return;

    // Software fallback: clear color texture
    if (framebuffer->color_texture && framebuffer->color_texture->data) {
        uint32_t* pixels = (uint32_t*)framebuffer->color_texture->data;
        uint32_t pixel_count = framebuffer->color_texture->width * framebuffer->color_texture->height;
        for (uint32_t i = 0; i < pixel_count; i++) {
            pixels[i] = color;
        }
    }

    // Call vendor-specific clear
    if (device->clear && framebuffer->driver_framebuffer) {
        device->clear(device->driver_data, framebuffer->driver_framebuffer, color);
    }
}

// 2D acceleration - Fill rectangle
void ugal_fill_rect(ugal_device_t* device, ugal_framebuffer_t* fb, int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color) {
    if (!device || !fb || !fb->color_texture) return;

    // Clamp to framebuffer bounds
    int32_t fb_width = (int32_t)fb->color_texture->width;
    int32_t fb_height = (int32_t)fb->color_texture->height;
    
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + (int32_t)width > fb_width) { width = fb_width - x; }
    if (y + (int32_t)height > fb_height) { height = fb_height - y; }
    
    if (width == 0 || height == 0) return;

    // Software fallback: fill pixels
    if (fb->color_texture->data) {
        uint32_t* pixels = (uint32_t*)fb->color_texture->data;
        for (uint32_t row = 0; row < height; row++) {
            uint32_t* row_ptr = &pixels[(y + row) * fb_width + x];
            for (uint32_t col = 0; col < width; col++) {
                row_ptr[col] = color;
            }
        }
    }

    // Call vendor-specific fill_rect
    if (device->fill_rect && fb->driver_framebuffer) {
        device->fill_rect(device->driver_data, fb->driver_framebuffer, x, y, width, height, color);
    }
}

// 2D acceleration - Draw line
void ugal_draw_line(ugal_device_t* device, ugal_framebuffer_t* fb, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color) {
    if (!device || !fb || !fb->color_texture) return;

    // Software fallback: Bresenham's line algorithm
    if (!fb->color_texture->data) return;

    int32_t fb_width = (int32_t)fb->color_texture->width;
    int32_t fb_height = (int32_t)fb->color_texture->height;
    uint32_t* pixels = (uint32_t*)fb->color_texture->data;

    int32_t dx = abs(x2 - x1);
    int32_t dy = abs(y2 - y1);
    int32_t sx = (x1 < x2) ? 1 : -1;
    int32_t sy = (y1 < y2) ? 1 : -1;
    int32_t err = dx - dy;

    int32_t x = x1;
    int32_t y = y1;

    while (true) {
        // Draw pixel if within bounds
        if (x >= 0 && x < fb_width && y >= 0 && y < fb_height) {
            pixels[y * fb_width + x] = color;
        }

        if (x == x2 && y == y2) break;

        int32_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }

    // Call vendor-specific line draw
    if (device->draw_line && fb->driver_framebuffer) {
        device->draw_line(device->driver_data, fb->driver_framebuffer, x1, y1, x2, y2, color);
    }
}

// 2D acceleration - Blit
void ugal_blit(ugal_device_t* device, ugal_texture_t* src, ugal_texture_t* dst, int32_t sx, int32_t sy, int32_t dx, int32_t dy, uint32_t width, uint32_t height) {
    if (!device || !src || !dst || !src->data || !dst->data) return;

    // Clamp source bounds
    if (sx < 0) { width += sx; dx -= sx; sx = 0; }
    if (sy < 0) { height += sy; dy -= sy; sy = 0; }
    if (sx + (int32_t)width > (int32_t)src->width) { width = src->width - sx; }
    if (sy + (int32_t)height > (int32_t)src->height) { height = src->height - sy; }

    // Clamp destination bounds
    if (dx < 0) { width += dx; sx -= dx; dx = 0; }
    if (dy < 0) { height += dy; sy -= sy; dy = 0; }
    if (dx + (int32_t)width > (int32_t)dst->width) { width = dst->width - dx; }
    if (dy + (int32_t)height > (int32_t)dst->height) { height = dst->height - dy; }

    if (width == 0 || height == 0) return;

    // Software fallback: copy pixels row by row
    uint32_t* src_pixels = (uint32_t*)src->data;
    uint32_t* dst_pixels = (uint32_t*)dst->data;

    for (uint32_t row = 0; row < height; row++) {
        uint32_t src_row = sy + row;
        uint32_t dst_row = dy + row;
        uint32_t src_offset = src_row * src->width + sx;
        uint32_t dst_offset = dst_row * dst->width + dx;
        memcpy(&dst_pixels[dst_offset], &src_pixels[src_offset], width * sizeof(uint32_t));
    }

    // Call vendor-specific blit
    if (device->blit && src->driver_texture && dst->driver_texture) { // Assuming driver_texture is needed for blit
        device->blit(device->driver_data, src->driver_texture, dst->driver_texture, sx, sy, dx, dy, width, height);
    }
}

#include "../../../libs/libc/include/syscall.h"

// Present framebuffer to display
void ugal_present(ugal_device_t* device, ugal_framebuffer_t* framebuffer) {
    if (!device || !framebuffer) return;

    // Call kernel to swap buffers (vsync)
    syscall(SYS_GFX_SWAP_BUFFERS, 0, 0, 0, 0, 0);

    // Call vendor-specific present
    if (device->present && framebuffer->driver_framebuffer) {
        device->present(device->driver_data, framebuffer->driver_framebuffer);
    }
}

// Attach color texture to framebuffer
void ugal_attach_color_texture(ugal_framebuffer_t* framebuffer, ugal_texture_t* texture) {
    if (!framebuffer || !texture) return;
    framebuffer->color_texture = texture;
}

// Attach depth texture to framebuffer
void ugal_attach_depth_texture(ugal_framebuffer_t* framebuffer, ugal_texture_t* texture) {
    if (!framebuffer || !texture) return;
    framebuffer->depth_texture = texture;
}

// Set VSync
void ugal_set_vsync(ugal_device_t* device, bool enable) {
    if (!device) return;

    // Call vendor-specific vsync
    if (device->set_vsync) {
        device->set_vsync(device->driver_data, enable);
    }
}