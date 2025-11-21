/**
 * @file gpu.h
 * @brief GPU driver framework interface
 */

#ifndef KERNEL_DRIVERS_GPU_GPU_H
#define KERNEL_DRIVERS_GPU_GPU_H

#include "../../types.h"
#include "../../errors.h"

// GPU device types
typedef enum {
    GPU_TYPE_VIRTIO,
    GPU_TYPE_INTEL,
    GPU_TYPE_AMD,
    GPU_TYPE_NVIDIA,
    GPU_TYPE_UNKNOWN
} gpu_type_t;

// GPU capabilities
#define GPU_CAP_2D         0x01
#define GPU_CAP_3D         0x02
#define GPU_CAP_COMPUTE    0x04
#define GPU_CAP_VIDEO      0x08

// GPU command types
typedef enum {
    GPU_CMD_CLEAR,
    GPU_CMD_DRAW_RECT,
    GPU_CMD_DRAW_LINE,
    GPU_CMD_COPY_BUFFER,
    GPU_CMD_FLUSH,
    GPU_CMD_SET_MODE
} gpu_command_type_t;

// GPU command structure
typedef struct {
    gpu_command_type_t type;
    uint32_t x, y, width, height;
    uint32_t color;
    void* data;
    size_t data_size;
} gpu_command_t;

// GPU mode structure
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;        // Bits per pixel
    uint32_t refresh_rate;
} gpu_mode_t;

// GPU device structure
typedef struct gpu_device {
    gpu_type_t type;
    char name[32];
    uint32_t capabilities;
    uint32_t* framebuffer;
    uint32_t framebuffer_size;
    gpu_mode_t current_mode;
    bool initialized;
    void* driver_data;  // Driver-specific data
    
    // Driver functions
    error_code_t (*init)(struct gpu_device* gpu);
    error_code_t (*set_mode)(struct gpu_device* gpu, const gpu_mode_t* mode);
    error_code_t (*submit_command)(struct gpu_device* gpu, const gpu_command_t* cmd);
    error_code_t (*flush)(struct gpu_device* gpu);
    void* (*get_framebuffer)(struct gpu_device* gpu);
    error_code_t (*deinit)(struct gpu_device* gpu);
    
    struct gpu_device* next;
} gpu_device_t;

// GPU framework functions
error_code_t gpu_init(void);
error_code_t gpu_register_device(gpu_device_t* device);
gpu_device_t* gpu_find_device(const char* name);
gpu_device_t* gpu_get_default(void);
error_code_t gpu_set_mode(gpu_device_t* gpu, const gpu_mode_t* mode);
error_code_t gpu_submit_command(gpu_device_t* gpu, const gpu_command_t* cmd);
error_code_t gpu_flush(gpu_device_t* gpu);
void* gpu_get_framebuffer(gpu_device_t* gpu);

#endif // KERNEL_DRIVERS_GPU_GPU_H

