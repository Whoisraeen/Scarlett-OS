/**
 * @file virtio_gpu.h
 * @brief VirtIO GPU driver
 */

#ifndef KERNEL_DRIVERS_VIRTIO_GPU_H
#define KERNEL_DRIVERS_VIRTIO_GPU_H

#include "../../include/types.h"
#include "../../include/errors.h"
#include "drivers/virtio.h"

// VirtIO GPU control queue
#define VIRTIO_GPU_CONTROL_QUEUE  0
#define VIRTIO_GPU_CURSOR_QUEUE   1

// VirtIO GPU request types
#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO    0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D  0x0101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF      0x0102
#define VIRTIO_GPU_CMD_SET_SCANOUT         0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH      0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D 0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106
#define VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING 0x0107
#define VIRTIO_GPU_CMD_UPDATE_CURSOR       0x0300
#define VIRTIO_GPU_CMD_MOVE_CURSOR         0x0301

// VirtIO GPU response types
#define VIRTIO_GPU_RESP_OK_NODATA          0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO    0x1101
#define VIRTIO_GPU_RESP_ERR_UNSPEC         0x1200
#define VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY  0x1201
#define VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID 0x1202
#define VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID 0x1203
#define VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID  0x1204

// VirtIO GPU pixel formats
#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM   1
#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM   2
#define VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM   3
#define VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM   4
#define VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM   5
#define VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM   6

// VirtIO GPU control request header
typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_ctrl_hdr_t;

// VirtIO GPU display info
typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    struct {
        struct {
            virtio_gpu_ctrl_hdr_t hdr;
            uint32_t pmodes[16];
            struct {
                uint32_t enabled;
                uint32_t flags;
                uint32_t r;
                uint32_t width;
                uint32_t height;
            } modes[16];
        } resp;
    } capsets;
} __attribute__((packed)) virtio_gpu_resp_display_info_t;

// VirtIO GPU resource create 2D
typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} __attribute__((packed)) virtio_gpu_resource_create_2d_t;

// VirtIO GPU set scanout
typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    struct {
        uint32_t x;
        uint32_t y;
        uint32_t width;
        uint32_t height;
    } r;
    uint32_t scanout_id;
    uint32_t resource_id;
} __attribute__((packed)) virtio_gpu_set_scanout_t;

// VirtIO GPU transfer to host 2D
typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    struct {
        uint32_t x;
        uint32_t y;
        uint32_t width;
        uint32_t height;
    } r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_transfer_to_host_2d_t;

// VirtIO GPU resource attach backing
typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
    struct {
        uint64_t addr;
        uint32_t length;
        uint32_t padding;
    } entries[];
} __attribute__((packed)) virtio_gpu_resource_attach_backing_t;

// VirtIO GPU resource flush
typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    struct {
        uint32_t x;
        uint32_t y;
        uint32_t width;
        uint32_t height;
    } r;
    uint32_t resource_id;
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_resource_flush_t;

// VirtIO GPU device
typedef struct {
    virtio_device_t* virtio_dev;
    uint32_t width;
    uint32_t height;
    uint32_t resource_id;
    void* framebuffer;
    bool initialized;
} virtio_gpu_t;

// VirtIO GPU functions
error_code_t virtio_gpu_init(virtio_gpu_t* gpu, uint64_t mmio_base);
error_code_t virtio_gpu_get_display_info(virtio_gpu_t* gpu);
error_code_t virtio_gpu_create_surface(virtio_gpu_t* gpu, uint32_t width, uint32_t height);
error_code_t virtio_gpu_flush(virtio_gpu_t* gpu, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void* virtio_gpu_get_framebuffer(virtio_gpu_t* gpu);
virtio_gpu_t* virtio_gpu_get(void);

#endif // KERNEL_DRIVERS_VIRTIO_GPU_H

