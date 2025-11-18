/**
 * @file block.h
 * @brief Block device interface
 * 
 * Abstract interface for block-oriented storage devices.
 */

#ifndef KERNEL_FS_BLOCK_H
#define KERNEL_FS_BLOCK_H

#include "../types.h"
#include "../errors.h"

// Block size (typically 512 bytes)
#define BLOCK_SIZE 512

// Block device structure
typedef struct block_device {
    const char* name;           // Device name (e.g., "sda", "hda")
    uint64_t block_count;      // Total number of blocks
    size_t block_size;          // Size of each block (usually 512)
    
    // Operations
    error_code_t (*read_block)(struct block_device* dev, uint64_t block_num, void* buffer);
    error_code_t (*write_block)(struct block_device* dev, uint64_t block_num, const void* buffer);
    error_code_t (*read_blocks)(struct block_device* dev, uint64_t start_block, uint64_t count, void* buffer);
    error_code_t (*write_blocks)(struct block_device* dev, uint64_t start_block, uint64_t count, const void* buffer);
    
    // Device-specific data
    void* private_data;
    
    // Linked list
    struct block_device* next;
} block_device_t;

// Block device functions
error_code_t block_device_init(void);
error_code_t block_device_register(block_device_t* device);
block_device_t* block_device_get(const char* name);
error_code_t block_device_read(block_device_t* dev, uint64_t block_num, void* buffer);
error_code_t block_device_write(block_device_t* dev, uint64_t block_num, const void* buffer);
error_code_t block_device_read_blocks(block_device_t* dev, uint64_t start, uint64_t count, void* buffer);
error_code_t block_device_write_blocks(block_device_t* dev, uint64_t start, uint64_t count, const void* buffer);

#endif // KERNEL_FS_BLOCK_H

