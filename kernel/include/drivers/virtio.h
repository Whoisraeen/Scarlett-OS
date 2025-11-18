/**
 * @file virtio.h
 * @brief VirtIO common definitions
 */

#ifndef KERNEL_DRIVERS_VIRTIO_H
#define KERNEL_DRIVERS_VIRTIO_H

#include "../../include/types.h"
#include "../../include/errors.h"

// VirtIO device IDs
#define VIRTIO_DEVICE_ID_GPU          16

// VirtIO status bits
#define VIRTIO_STATUS_ACKNOWLEDGE     1
#define VIRTIO_STATUS_DRIVER          2
#define VIRTIO_STATUS_FAILED          128
#define VIRTIO_STATUS_FEATURES_OK     8
#define VIRTIO_STATUS_DRIVER_OK       4

// VirtIO MMIO base offsets
#define VIRTIO_MMIO_MAGIC_VALUE       0x000
#define VIRTIO_MMIO_VERSION           0x004
#define VIRTIO_MMIO_DEVICE_ID         0x008
#define VIRTIO_MMIO_VENDOR_ID         0x00C
#define VIRTIO_MMIO_DEVICE_FEATURES   0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES   0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL         0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX     0x034
#define VIRTIO_MMIO_QUEUE_NUM         0x038
#define VIRTIO_MMIO_QUEUE_READY       0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY      0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS  0x060
#define VIRTIO_MMIO_INTERRUPT_ACK     0x064
#define VIRTIO_MMIO_STATUS            0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW    0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH   0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW   0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH  0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW    0x0A0
#define VIRTIO_MMIO_QUEUE_USED_HIGH   0x0A4

// VirtIO queue
typedef struct {
    void* desc;      // Descriptor table
    void* avail;     // Available ring
    void* used;      // Used ring
    uint16_t size;   // Queue size
    uint16_t index;  // Queue index
    bool ready;      // Is queue ready?
} virtio_queue_t;

// VirtIO device
typedef struct {
    uint64_t mmio_base;  // MMIO base address
    uint32_t device_id;  // Device ID
    uint32_t version;    // Version
    virtio_queue_t* queues;  // Virtqueues
    uint16_t queue_count;    // Number of queues
    bool initialized;        // Is device initialized?
} virtio_device_t;

// VirtIO functions
error_code_t virtio_init(virtio_device_t* dev, uint64_t mmio_base);
error_code_t virtio_queue_init(virtio_device_t* dev, uint16_t queue_index, uint16_t queue_size);
error_code_t virtio_queue_notify(virtio_device_t* dev, uint16_t queue_index);
void* virtio_alloc_queue_memory(uint16_t queue_size);

#endif // KERNEL_DRIVERS_VIRTIO_H

