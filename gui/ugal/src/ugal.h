/**
 * @file ugal.h
 * @brief Universal GPU Abstraction Layer (UGAL)
 *
 * Unified API for all GPUs (NVIDIA, AMD, Intel, Apple Silicon)
 * Provides hardware-accelerated graphics with vendor-agnostic interface
 */

#ifndef GUI_UGAL_H
#define GUI_UGAL_H

#include <stdint.h>
#include <stdbool.h>

// UGAL API version
#define UGAL_VERSION_MAJOR 1
#define UGAL_VERSION_MINOR 0

// GPU vendor IDs
typedef enum {
    UGAL_VENDOR_UNKNOWN = 0,
    UGAL_VENDOR_NVIDIA = 1,
    UGAL_VENDOR_AMD = 2,
    UGAL_VENDOR_INTEL = 3,
    UGAL_VENDOR_APPLE = 4,
    UGAL_VENDOR_VIRTIO = 5,
} ugal_vendor_t;

// GPU capabilities
typedef enum {
    UGAL_CAP_2D_ACCEL = 0x01,
    UGAL_CAP_3D_ACCEL = 0x02,
    UGAL_CAP_COMPUTE = 0x04,
    UGAL_CAP_VIDEO_DECODE = 0x08,
    UGAL_CAP_VIDEO_ENCODE = 0x10,
    UGAL_CAP_DISPLAY_OUTPUT = 0x20,
    UGAL_CAP_MULTI_MONITOR = 0x40,
} ugal_capabilities_t;

// Buffer usage flags
typedef enum {
    UGAL_BUFFER_USAGE_VERTEX = 0x01,
    UGAL_BUFFER_USAGE_INDEX = 0x02,
    UGAL_BUFFER_USAGE_UNIFORM = 0x04,
    UGAL_BUFFER_USAGE_STORAGE = 0x08,
    UGAL_BUFFER_USAGE_TRANSFER_SRC = 0x10,
    UGAL_BUFFER_USAGE_TRANSFER_DST = 0x20,
} ugal_buffer_usage_t;

// Texture formats
typedef enum {
    UGAL_FORMAT_RGBA8 = 0,
    UGAL_FORMAT_BGRA8 = 1,
    UGAL_FORMAT_RGB8 = 2,
    UGAL_FORMAT_R8 = 3,
    UGAL_FORMAT_DEPTH24_STENCIL8 = 4,
} ugal_format_t;

// Primitive topology
typedef enum {
    UGAL_TOPOLOGY_POINT_LIST = 0,
    UGAL_TOPOLOGY_LINE_LIST = 1,
    UGAL_TOPOLOGY_LINE_STRIP = 2,
    UGAL_TOPOLOGY_TRIANGLE_LIST = 3,
    UGAL_TOPOLOGY_TRIANGLE_STRIP = 4,
} ugal_topology_t;

// GPU device information
typedef struct {
    ugal_vendor_t vendor;
    uint32_t device_id;
    char name[128];
    uint64_t vram_size;
    uint32_t capabilities;
    uint32_t max_texture_size;
    uint32_t max_render_targets;
} ugal_device_info_t;

// Forward declarations
typedef struct ugal_device ugal_device_t;
typedef struct ugal_buffer ugal_buffer_t;
typedef struct ugal_texture ugal_texture_t;
typedef struct ugal_framebuffer ugal_framebuffer_t;
typedef struct ugal_pipeline ugal_pipeline_t;
typedef struct ugal_command_buffer ugal_command_buffer_t;

// Device management
ugal_device_t* ugal_create_device(uint32_t device_index);
void ugal_destroy_device(ugal_device_t* device);
void ugal_get_device_info(ugal_device_t* device, ugal_device_info_t* info);
uint32_t ugal_enumerate_devices(ugal_device_info_t* devices, uint32_t max_devices);

// Buffer operations
ugal_buffer_t* ugal_create_buffer(ugal_device_t* device, uint64_t size, uint32_t usage);
void ugal_destroy_buffer(ugal_buffer_t* buffer);
void* ugal_map_buffer(ugal_buffer_t* buffer);
void ugal_unmap_buffer(ugal_buffer_t* buffer);
void ugal_update_buffer(ugal_buffer_t* buffer, const void* data, uint64_t offset, uint64_t size);

// Texture operations
ugal_texture_t* ugal_create_texture(ugal_device_t* device, uint32_t width, uint32_t height, ugal_format_t format);
void ugal_destroy_texture(ugal_texture_t* texture);
void ugal_update_texture(ugal_texture_t* texture, const void* data, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

// Framebuffer operations
ugal_framebuffer_t* ugal_create_framebuffer(ugal_device_t* device, uint32_t width, uint32_t height);
void ugal_destroy_framebuffer(ugal_framebuffer_t* framebuffer);
void ugal_attach_color_texture(ugal_framebuffer_t* framebuffer, ugal_texture_t* texture);
void ugal_attach_depth_texture(ugal_framebuffer_t* framebuffer, ugal_texture_t* texture);

// Pipeline operations
ugal_pipeline_t* ugal_create_pipeline(ugal_device_t* device);
void ugal_destroy_pipeline(ugal_pipeline_t* pipeline);
void ugal_set_vertex_shader(ugal_pipeline_t* pipeline, const char* shader_code);
void ugal_set_fragment_shader(ugal_pipeline_t* pipeline, const char* shader_code);

// Command buffer operations
ugal_command_buffer_t* ugal_create_command_buffer(ugal_device_t* device);
void ugal_destroy_command_buffer(ugal_command_buffer_t* cmd);
void ugal_begin_command_buffer(ugal_command_buffer_t* cmd);
void ugal_end_command_buffer(ugal_command_buffer_t* cmd);
void ugal_submit_command_buffer(ugal_device_t* device, ugal_command_buffer_t* cmd);

// Drawing commands
void ugal_cmd_begin_render_pass(ugal_command_buffer_t* cmd, ugal_framebuffer_t* framebuffer);
void ugal_cmd_end_render_pass(ugal_command_buffer_t* cmd);
void ugal_cmd_bind_pipeline(ugal_command_buffer_t* cmd, ugal_pipeline_t* pipeline);
void ugal_cmd_bind_vertex_buffer(ugal_command_buffer_t* cmd, ugal_buffer_t* buffer);
void ugal_cmd_bind_index_buffer(ugal_command_buffer_t* cmd, ugal_buffer_t* buffer);
void ugal_cmd_draw(ugal_command_buffer_t* cmd, uint32_t vertex_count, uint32_t first_vertex);
void ugal_cmd_draw_indexed(ugal_command_buffer_t* cmd, uint32_t index_count, uint32_t first_index);

// 2D acceleration (high-level API)
void ugal_clear(ugal_device_t* device, ugal_framebuffer_t* framebuffer, uint32_t color);
void ugal_blit(ugal_device_t* device, ugal_texture_t* src, ugal_texture_t* dst, int32_t sx, int32_t sy, int32_t dx, int32_t dy, uint32_t width, uint32_t height);
void ugal_fill_rect(ugal_device_t* device, ugal_framebuffer_t* fb, int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color);
void ugal_draw_line(ugal_device_t* device, ugal_framebuffer_t* fb, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);

// Display output
void ugal_present(ugal_device_t* device, ugal_framebuffer_t* framebuffer);
void ugal_set_vsync(ugal_device_t* device, bool enable);

#endif // GUI_UGAL_H
