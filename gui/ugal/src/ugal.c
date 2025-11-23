/**
 * @file ugal.c
 * @brief Universal GPU Abstraction Layer Implementation
 */

#include "ugal.h"
#include <string.h>

// Maximum number of GPU devices
#define MAX_GPU_DEVICES 8

// UGAL device structure
struct ugal_device {
    ugal_device_info_t info;
    void* driver_data;  // Vendor-specific driver data

    // Function pointers for vendor-specific operations
    void* (*create_buffer)(void* driver_data, uint64_t size, uint32_t usage);
    void (*destroy_buffer)(void* driver_data, void* buffer);
    void* (*map_buffer)(void* driver_data, void* buffer);
    void (*unmap_buffer)(void* driver_data, void* buffer);

    void* (*create_texture)(void* driver_data, uint32_t width, uint32_t height, ugal_format_t format);
    void (*destroy_texture)(void* driver_data, void* texture);

    void (*present)(void* driver_data, void* framebuffer);
    void (*clear)(void* driver_data, void* framebuffer, uint32_t color);
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

// UGAL pipeline structure
struct ugal_pipeline {
    ugal_device_t* device;
    void* driver_pipeline;
    char* vertex_shader;
    char* fragment_shader;
};

// UGAL command buffer structure
struct ugal_command_buffer {
    ugal_device_t* device;
    void* driver_cmd;
    bool recording;
};

// Global device list
static ugal_device_t* g_devices[MAX_GPU_DEVICES] = {NULL};
static uint32_t g_device_count = 0;

// Enumerate available GPU devices
uint32_t ugal_enumerate_devices(ugal_device_info_t* devices, uint32_t max_devices) {
    // TODO: Query PCI for GPU devices
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

    // TODO: Initialize vendor-specific driver

    g_devices[g_device_count++] = device;

    return device;
}

// Destroy UGAL device
void ugal_destroy_device(ugal_device_t* device) {
    if (!device) return;

    // TODO: Cleanup vendor-specific driver

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

    // TODO: Call vendor-specific buffer creation
    buffer->driver_buffer = NULL;

    return buffer;
}

// Destroy buffer
void ugal_destroy_buffer(ugal_buffer_t* buffer) {
    if (!buffer) return;

    // TODO: Call vendor-specific buffer destruction

    free(buffer);
}

// Map buffer
void* ugal_map_buffer(ugal_buffer_t* buffer) {
    if (!buffer || !buffer->device) return NULL;

    // TODO: Call vendor-specific map
    return NULL;
}

// Unmap buffer
void ugal_unmap_buffer(ugal_buffer_t* buffer) {
    if (!buffer || !buffer->device) return;

    // TODO: Call vendor-specific unmap
}

// Update buffer
void ugal_update_buffer(ugal_buffer_t* buffer, const void* data, uint64_t offset, uint64_t size) {
    if (!buffer || !data) return;

    // TODO: Call vendor-specific update
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

    // TODO: Call vendor-specific texture creation when driver available

    return texture;
}

// Destroy texture
void ugal_destroy_texture(ugal_texture_t* texture) {
    if (!texture) return;

    // TODO: Call vendor-specific texture destruction when driver available

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

    // TODO: Call vendor-specific texture update when driver available
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

    // TODO: Call vendor-specific framebuffer creation
    framebuffer->driver_framebuffer = NULL;

    return framebuffer;
}

// Destroy framebuffer
void ugal_destroy_framebuffer(ugal_framebuffer_t* framebuffer) {
    if (!framebuffer) return;

    // TODO: Call vendor-specific framebuffer destruction

    free(framebuffer);
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

    // TODO: Call vendor-specific clear when driver available
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

    // TODO: Call vendor-specific fill when driver available
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

    // TODO: Call vendor-specific line draw when driver available
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
    if (dy < 0) { height += dy; sy -= dy; dy = 0; }
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

    // TODO: Call vendor-specific blit when driver available
}

// Present framebuffer to display
void ugal_present(ugal_device_t* device, ugal_framebuffer_t* framebuffer) {
    if (!device || !framebuffer) return;

    // TODO: Call vendor-specific present
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

    // TODO: Call vendor-specific vsync
    // For now, no-op (software rendering doesn't need vsync)
}
