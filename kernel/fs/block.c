/**
 * @file block.c
 * @brief Block device implementation
 */

#include "../include/types.h"
#include "../include/fs/block.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

// Block device list
static block_device_t* block_devices = NULL;

/**
 * Initialize block device system
 */
error_code_t block_device_init(void) {
    kinfo("Initializing block device system...\n");
    
    block_devices = NULL;
    
    kinfo("Block device system initialized\n");
    return ERR_OK;
}

/**
 * Register a block device
 */
error_code_t block_device_register(block_device_t* device) {
    if (!device) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("Registering block device: %s (%lu blocks, %lu bytes/block)\n",
          device->name, device->block_count, device->block_size);
    
    // Add to list
    device->next = block_devices;
    block_devices = device;
    
    return ERR_OK;
}

/**
 * Get block device by name
 */
block_device_t* block_device_get(const char* name) {
    if (!name) {
        return NULL;
    }
    
    for (block_device_t* dev = block_devices; dev != NULL; dev = dev->next) {
        // Simple string comparison
        const char* a = dev->name;
        const char* b = name;
        bool match = true;
        while (*a && *b) {
            if (*a != *b) {
                match = false;
                break;
            }
            a++;
            b++;
        }
        if (match && *a == '\0' && *b == '\0') {
            return dev;
        }
    }
    
    return NULL;
}

/**
 * Read a single block
 */
error_code_t block_device_read(block_device_t* dev, uint64_t block_num, void* buffer) {
    if (!dev || !buffer) {
        return ERR_INVALID_ARG;
    }
    
    if (block_num >= dev->block_count) {
        return ERR_INVALID_ARG;
    }
    
    if (!dev->read_block) {
        return ERR_NOT_SUPPORTED;
    }
    
    return dev->read_block(dev, block_num, buffer);
}

/**
 * Write a single block
 */
error_code_t block_device_write(block_device_t* dev, uint64_t block_num, const void* buffer) {
    if (!dev || !buffer) {
        return ERR_INVALID_ARG;
    }
    
    if (block_num >= dev->block_count) {
        return ERR_INVALID_ARG;
    }
    
    if (!dev->write_block) {
        return ERR_NOT_SUPPORTED;
    }
    
    return dev->write_block(dev, block_num, buffer);
}

/**
 * Read multiple blocks
 */
error_code_t block_device_read_blocks(block_device_t* dev, uint64_t start, uint64_t count, void* buffer) {
    if (!dev || !buffer || count == 0) {
        return ERR_INVALID_ARG;
    }
    
    if (start + count > dev->block_count) {
        return ERR_INVALID_ARG;
    }
    
    // Use optimized multi-block read if available
    if (dev->read_blocks) {
        return dev->read_blocks(dev, start, count, buffer);
    }
    
    // Otherwise, read block by block
    uint8_t* buf = (uint8_t*)buffer;
    for (uint64_t i = 0; i < count; i++) {
        error_code_t err = block_device_read(dev, start + i, buf + (i * dev->block_size));
        if (err != ERR_OK) {
            return err;
        }
    }
    
    return ERR_OK;
}

/**
 * Write multiple blocks
 */
error_code_t block_device_write_blocks(block_device_t* dev, uint64_t start, uint64_t count, const void* buffer) {
    if (!dev || !buffer || count == 0) {
        return ERR_INVALID_ARG;
    }
    
    if (start + count > dev->block_count) {
        return ERR_INVALID_ARG;
    }
    
    // Use optimized multi-block write if available
    if (dev->write_blocks) {
        return dev->write_blocks(dev, start, count, buffer);
    }
    
    // Otherwise, write block by block
    const uint8_t* buf = (const uint8_t*)buffer;
    for (uint64_t i = 0; i < count; i++) {
        error_code_t err = block_device_write(dev, start + i, buf + (i * dev->block_size));
        if (err != ERR_OK) {
            return err;
        }
    }
    
    return ERR_OK;
}

