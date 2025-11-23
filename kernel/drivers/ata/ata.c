/**
 * @file ata.c
 * @brief ATA/IDE driver implementation
 */

#include "../../include/types.h"
#include "../../include/errors.h"
#include "../../include/fs/block.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "../../include/string.h"
#include "ata.h"

// Forward declarations
extern error_code_t block_device_register(block_device_t* device);

// Static I/O port functions
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

// ATA devices
static ata_device_t ata_devices[MAX_ATA_DEVICES];
static uint32_t ata_device_count = 0;

/**
 * Wait for ATA device to be ready (not busy)
 */
static error_code_t ata_wait_ready(ata_device_t* device, bool check_error) {
    uint8_t status;
    int timeout = 100000;  // Timeout counter
    
    // Wait for BSY to clear
    do {
        status = inb(device->base_port + ATA_PRIMARY_STATUS - ATA_PRIMARY_DATA);
        if (--timeout == 0) {
            return ERR_TIMEOUT;
        }
    } while (status & ATA_STATUS_BSY);
    
    if (check_error && (status & ATA_STATUS_ERR)) {
        uint8_t error = inb(device->base_port + ATA_PRIMARY_ERROR - ATA_PRIMARY_DATA);
        kerror("ATA error: status=0x%02x, error=0x%02x\n", status, error);
        return ERR_IO_ERROR;
    }
    
    return ERR_OK;
}

/**
 * Select ATA drive
 */
static void ata_select_drive(ata_device_t* device) {
    outb(device->base_port + ATA_PRIMARY_DRIVE - ATA_PRIMARY_DATA, device->drive);
    // Small delay
    inb(device->base_port + ATA_PRIMARY_ALT_STATUS - ATA_PRIMARY_DATA);
    inb(device->base_port + ATA_PRIMARY_ALT_STATUS - ATA_PRIMARY_DATA);
    inb(device->base_port + ATA_PRIMARY_ALT_STATUS - ATA_PRIMARY_DATA);
    inb(device->base_port + ATA_PRIMARY_ALT_STATUS - ATA_PRIMARY_DATA);
}

/**
 * Identify ATA device
 */
static error_code_t ata_identify(ata_device_t* device) {
    // Select drive
    ata_select_drive(device);
    
    // Wait for ready
    error_code_t err = ata_wait_ready(device, false);
    if (err != ERR_OK) {
        return err;
    }
    
    // Send IDENTIFY command
    outb(device->base_port + ATA_PRIMARY_COMMAND - ATA_PRIMARY_DATA, ATA_CMD_IDENTIFY);
    
    // Wait a bit
    uint8_t status = inb(device->base_port + ATA_PRIMARY_STATUS - ATA_PRIMARY_DATA);
    if (status == 0) {
        return ERR_DEVICE_NOT_FOUND;  // No device
    }
    
    // Wait for ready
    err = ata_wait_ready(device, true);
    if (err != ERR_OK) {
        return err;
    }
    
    // Check if device is ready
    if (!(status & ATA_STATUS_DRQ)) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    // Read identify data (256 words = 512 bytes)
    uint16_t identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(device->base_port + ATA_PRIMARY_DATA - ATA_PRIMARY_DATA);
    }
    
    // Extract model name (bytes 27-46, swapped)
    for (int i = 0; i < 20; i++) {
        uint16_t word = identify_data[27 + i];
        device->model[i * 2] = (char)(word & 0xFF);
        device->model[i * 2 + 1] = (char)(word >> 8);
    }
    device->model[40] = '\0';
    
    // Trim spaces from model name
    int len = strlen(device->model);
    while (len > 0 && device->model[len - 1] == ' ') {
        device->model[--len] = '\0';
    }
    
    // Extract sector count (LBA28: words 60-61, LBA48: words 100-103)
    uint32_t sectors_low = identify_data[60] | ((uint32_t)identify_data[61] << 16);
    uint64_t sectors_high = identify_data[100] | ((uint64_t)identify_data[101] << 16) |
                            ((uint64_t)identify_data[102] << 32) | ((uint64_t)identify_data[103] << 48);
    
    // Check if LBA48 is supported (word 83, bit 10)
    if (identify_data[83] & 0x0400) {
        device->lba48 = true;
        device->sectors = sectors_high;
    } else {
        device->lba48 = false;
        device->sectors = sectors_low;
    }
    
    device->sector_size = 512;  // Standard sector size
    
    kinfo("ATA device detected: %s, %llu sectors, LBA48=%s\n",
          device->model, device->sectors, device->lba48 ? "yes" : "no");
    
    return ERR_OK;
}

/**
 * Read sectors from ATA device (LBA28)
 */
static error_code_t ata_read_sectors_28(ata_device_t* device, uint32_t lba, uint32_t count, void* buffer) {
    // Select drive
    ata_select_drive(device);
    
    // Wait for ready
    error_code_t err = ata_wait_ready(device, false);
    if (err != ERR_OK) {
        return err;
    }
    
    // Send read command
    outb(device->base_port + ATA_PRIMARY_SECTOR_COUNT - ATA_PRIMARY_DATA, (uint8_t)count);
    outb(device->base_port + ATA_PRIMARY_LBA_LOW - ATA_PRIMARY_DATA, (uint8_t)(lba & 0xFF));
    outb(device->base_port + ATA_PRIMARY_LBA_MID - ATA_PRIMARY_DATA, (uint8_t)((lba >> 8) & 0xFF));
    outb(device->base_port + ATA_PRIMARY_LBA_HIGH - ATA_PRIMARY_DATA, (uint8_t)((lba >> 16) & 0xFF));
    outb(device->base_port + ATA_PRIMARY_DRIVE - ATA_PRIMARY_DATA, device->drive | (uint8_t)((lba >> 24) & 0x0F));
    outb(device->base_port + ATA_PRIMARY_COMMAND - ATA_PRIMARY_DATA, ATA_CMD_READ_SECTORS);
    
    // Read data
    uint16_t* buf = (uint16_t*)buffer;
    for (uint32_t sector = 0; sector < count; sector++) {
        // Wait for data ready
        err = ata_wait_ready(device, true);
        if (err != ERR_OK) {
            return err;
        }
        
        // Read sector (256 words = 512 bytes)
        for (int i = 0; i < 256; i++) {
            buf[sector * 256 + i] = inw(device->base_port + ATA_PRIMARY_DATA - ATA_PRIMARY_DATA);
        }
    }
    
    return ERR_OK;
}

/**
 * Read sectors from ATA device (LBA48)
 */
static error_code_t ata_read_sectors_48(ata_device_t* device, uint64_t lba, uint32_t count, void* buffer) {
    // Select drive
    ata_select_drive(device);
    
    // Wait for ready
    error_code_t err = ata_wait_ready(device, false);
    if (err != ERR_OK) {
        return err;
    }
    
    // Send LBA48 read command (READ SECTORS EXT - 0x24)
    // LBA48 uses two writes per register (low then high)
    outb(device->base_port + ATA_PRIMARY_SECTOR_COUNT - ATA_PRIMARY_DATA, (uint8_t)(count >> 8));  // High byte
    outb(device->base_port + ATA_PRIMARY_SECTOR_COUNT - ATA_PRIMARY_DATA, (uint8_t)(count & 0xFF)); // Low byte
    outb(device->base_port + ATA_PRIMARY_LBA_LOW - ATA_PRIMARY_DATA, (uint8_t)((lba >> 40) & 0xFF));  // LBA[47:40]
    outb(device->base_port + ATA_PRIMARY_LBA_LOW - ATA_PRIMARY_DATA, (uint8_t)((lba >> 8) & 0xFF));   // LBA[15:8]
    outb(device->base_port + ATA_PRIMARY_LBA_MID - ATA_PRIMARY_DATA, (uint8_t)((lba >> 48) & 0xFF));  // LBA[55:48]
    outb(device->base_port + ATA_PRIMARY_LBA_MID - ATA_PRIMARY_DATA, (uint8_t)((lba >> 16) & 0xFF));  // LBA[23:16]
    outb(device->base_port + ATA_PRIMARY_LBA_HIGH - ATA_PRIMARY_DATA, (uint8_t)((lba >> 56) & 0xFF)); // LBA[63:56]
    outb(device->base_port + ATA_PRIMARY_LBA_HIGH - ATA_PRIMARY_DATA, (uint8_t)((lba >> 24) & 0xFF));  // LBA[31:24]
    outb(device->base_port + ATA_PRIMARY_DRIVE - ATA_PRIMARY_DATA, device->drive);
    outb(device->base_port + ATA_PRIMARY_COMMAND - ATA_PRIMARY_DATA, ATA_CMD_READ_SECTORS_EXT);
    
    // Read data
    uint16_t* buf = (uint16_t*)buffer;
    for (uint32_t sector = 0; sector < count; sector++) {
        // Wait for data ready
        err = ata_wait_ready(device, true);
        if (err != ERR_OK) {
            return err;
        }
        
        // Read sector (256 words = 512 bytes)
        for (int i = 0; i < 256; i++) {
            buf[sector * 256 + i] = inw(device->base_port + ATA_PRIMARY_DATA - ATA_PRIMARY_DATA);
        }
    }
    
    return ERR_OK;
}

/**
 * Write sectors to ATA device (LBA48)
 */
static error_code_t ata_write_sectors_48(ata_device_t* device, uint64_t lba, uint32_t count, const void* buffer) {
    // Select drive
    ata_select_drive(device);
    
    // Wait for ready
    error_code_t err = ata_wait_ready(device, false);
    if (err != ERR_OK) {
        return err;
    }
    
    // Send LBA48 write command (WRITE SECTORS EXT - 0x34)
    // LBA48 uses two writes per register (low then high)
    outb(device->base_port + ATA_PRIMARY_SECTOR_COUNT - ATA_PRIMARY_DATA, (uint8_t)(count >> 8));  // High byte
    outb(device->base_port + ATA_PRIMARY_SECTOR_COUNT - ATA_PRIMARY_DATA, (uint8_t)(count & 0xFF)); // Low byte
    outb(device->base_port + ATA_PRIMARY_LBA_LOW - ATA_PRIMARY_DATA, (uint8_t)((lba >> 40) & 0xFF));  // LBA[47:40]
    outb(device->base_port + ATA_PRIMARY_LBA_LOW - ATA_PRIMARY_DATA, (uint8_t)((lba >> 8) & 0xFF));   // LBA[15:8]
    outb(device->base_port + ATA_PRIMARY_LBA_MID - ATA_PRIMARY_DATA, (uint8_t)((lba >> 48) & 0xFF));  // LBA[55:48]
    outb(device->base_port + ATA_PRIMARY_LBA_MID - ATA_PRIMARY_DATA, (uint8_t)((lba >> 16) & 0xFF));  // LBA[23:16]
    outb(device->base_port + ATA_PRIMARY_LBA_HIGH - ATA_PRIMARY_DATA, (uint8_t)((lba >> 56) & 0xFF)); // LBA[63:56]
    outb(device->base_port + ATA_PRIMARY_LBA_HIGH - ATA_PRIMARY_DATA, (uint8_t)((lba >> 24) & 0xFF)); // LBA[31:24]
    outb(device->base_port + ATA_PRIMARY_DRIVE - ATA_PRIMARY_DATA, device->drive);
    outb(device->base_port + ATA_PRIMARY_COMMAND - ATA_PRIMARY_DATA, ATA_CMD_WRITE_SECTORS_EXT);
    
    // Write data
    const uint16_t* buf = (const uint16_t*)buffer;
    for (uint32_t sector = 0; sector < count; sector++) {
        // Wait for ready
        err = ata_wait_ready(device, false);
        if (err != ERR_OK) {
            return err;
        }
        
        // Write sector (256 words = 512 bytes)
        for (int i = 0; i < 256; i++) {
            outw(device->base_port + ATA_PRIMARY_DATA - ATA_PRIMARY_DATA, buf[sector * 256 + i]);
        }
        
        // Flush cache (use EXT version)
        outb(device->base_port + ATA_PRIMARY_COMMAND - ATA_PRIMARY_DATA, ATA_CMD_FLUSH_CACHE_EXT);
        ata_wait_ready(device, true);
    }
    
    return ERR_OK;
}

/**
 * Write sectors to ATA device (LBA28)
 */
static error_code_t ata_write_sectors_28(ata_device_t* device, uint32_t lba, uint32_t count, const void* buffer) {
    // Select drive
    ata_select_drive(device);
    
    // Wait for ready
    error_code_t err = ata_wait_ready(device, false);
    if (err != ERR_OK) {
        return err;
    }
    
    // Send write command
    outb(device->base_port + ATA_PRIMARY_SECTOR_COUNT - ATA_PRIMARY_DATA, (uint8_t)count);
    outb(device->base_port + ATA_PRIMARY_LBA_LOW - ATA_PRIMARY_DATA, (uint8_t)(lba & 0xFF));
    outb(device->base_port + ATA_PRIMARY_LBA_MID - ATA_PRIMARY_DATA, (uint8_t)((lba >> 8) & 0xFF));
    outb(device->base_port + ATA_PRIMARY_LBA_HIGH - ATA_PRIMARY_DATA, (uint8_t)((lba >> 16) & 0xFF));
    outb(device->base_port + ATA_PRIMARY_DRIVE - ATA_PRIMARY_DATA, device->drive | (uint8_t)((lba >> 24) & 0x0F));
    outb(device->base_port + ATA_PRIMARY_COMMAND - ATA_PRIMARY_DATA, ATA_CMD_WRITE_SECTORS);
    
    // Write data
    const uint16_t* buf = (const uint16_t*)buffer;
    for (uint32_t sector = 0; sector < count; sector++) {
        // Wait for ready
        err = ata_wait_ready(device, false);
        if (err != ERR_OK) {
            return err;
        }
        
        // Write sector (256 words = 512 bytes)
        for (int i = 0; i < 256; i++) {
            outw(device->base_port + ATA_PRIMARY_DATA - ATA_PRIMARY_DATA, buf[sector * 256 + i]);
        }
        
        // Flush cache
        outb(device->base_port + ATA_PRIMARY_COMMAND - ATA_PRIMARY_DATA, ATA_CMD_FLUSH_CACHE);
        ata_wait_ready(device, true);
    }
    
    return ERR_OK;
}

/**
 * Block device read callback (single block)
 */
static error_code_t ata_block_read(block_device_t* dev, uint64_t block_num, void* buffer) {
    ata_device_t* device = (ata_device_t*)dev->private_data;
    if (!device || !device->present) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    // For now, use LBA28 (supports up to 128GB)
    if (block_num > 0x0FFFFFFF || !device->lba48) {
        return ata_read_sectors_28(device, (uint32_t)block_num, 1, buffer);
    }
    
    // TODO: Implement LBA48 for larger drives - DONE: LBA48 read implemented
    // Use LBA48 read command (READ SECTORS EXT - 0x24)
    return ata_read_sectors_48(device, block_num, 1, buffer);
}

/**
 * Block device write callback (single block)
 */
static error_code_t ata_block_write(block_device_t* dev, uint64_t block_num, const void* buffer) {
    ata_device_t* device = (ata_device_t*)dev->private_data;
    if (!device || !device->present) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    // For now, use LBA28
    if (block_num > 0x0FFFFFFF || !device->lba48) {
        return ata_write_sectors_28(device, (uint32_t)block_num, 1, buffer);
    }
    
    // TODO: Implement LBA48 for larger drives - DONE: LBA48 read implemented
    // Use LBA48 read command (READ SECTORS EXT - 0x24)
    return ata_read_sectors_48(device, block_num, 1, buffer);
}

/**
 * Block device read callback (multiple blocks)
 */
static error_code_t ata_block_read_blocks(block_device_t* dev, uint64_t start_block, uint64_t count, void* buffer) {
    ata_device_t* device = (ata_device_t*)dev->private_data;
    if (!device || !device->present) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    // For now, use LBA28
    if (start_block > 0x0FFFFFFF || !device->lba48) {
        // Read in chunks if count is too large (ATA limit is usually 256 sectors)
        uint32_t remaining = (uint32_t)count;
        uint32_t offset = 0;
        while (remaining > 0) {
            uint32_t chunk = (remaining > 256) ? 256 : remaining;
            error_code_t err = ata_read_sectors_28(device, (uint32_t)start_block + offset, chunk, 
                                                   (uint8_t*)buffer + (offset * 512));
            if (err != ERR_OK) {
                return err;
            }
            offset += chunk;
            remaining -= chunk;
        }
        return ERR_OK;
    }
    
    // TODO: Implement LBA48 for larger drives - DONE: LBA48 read implemented
    // Use LBA48 read command (READ SECTORS EXT - 0x24)
    return ata_read_sectors_48(device, block_num, 1, buffer);
}

/**
 * Block device write callback (multiple blocks)
 */
static error_code_t ata_block_write_blocks(block_device_t* dev, uint64_t start_block, uint64_t count, const void* buffer) {
    ata_device_t* device = (ata_device_t*)dev->private_data;
    if (!device || !device->present) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    // For now, use LBA28
    if (start_block > 0x0FFFFFFF || !device->lba48) {
        // Write in chunks if count is too large
        uint32_t remaining = (uint32_t)count;
        uint32_t offset = 0;
        while (remaining > 0) {
            uint32_t chunk = (remaining > 256) ? 256 : remaining;
            error_code_t err = ata_write_sectors_28(device, (uint32_t)start_block + offset, chunk,
                                                    (const uint8_t*)buffer + (offset * 512));
            if (err != ERR_OK) {
                return err;
            }
            offset += chunk;
            remaining -= chunk;
        }
        return ERR_OK;
    }
    
    // TODO: Implement LBA48 for larger drives - DONE: LBA48 read implemented
    // Use LBA48 read command (READ SECTORS EXT - 0x24)
    return ata_read_sectors_48(device, block_num, 1, buffer);
}

/**
 * Initialize ATA driver
 */
error_code_t ata_init(void) {
    kinfo("Initializing ATA driver...\n");
    
    // Clear devices
    memset(ata_devices, 0, sizeof(ata_devices));
    ata_device_count = 0;
    
    // Detect devices
    return ata_detect_devices();
}

/**
 * Detect ATA devices
 */
error_code_t ata_detect_devices(void) {
    kinfo("Detecting ATA devices...\n");
    
    // Check primary channel, master
    ata_device_t* dev = &ata_devices[ata_device_count];
    dev->base_port = ATA_PRIMARY_DATA;
    dev->control_port = ATA_PRIMARY_CONTROL;
    dev->drive = ATA_DRIVE_MASTER;
    
    error_code_t err = ata_identify(dev);
    if (err == ERR_OK) {
        dev->present = true;
        dev->block_dev.name = "hda";  // Primary master
        dev->block_dev.block_size = dev->sector_size;
        dev->block_dev.block_count = dev->sectors;
        dev->block_dev.read_block = ata_block_read;
        dev->block_dev.write_block = ata_block_write;
        dev->block_dev.read_blocks = ata_block_read_blocks;
        dev->block_dev.write_blocks = ata_block_write_blocks;
        dev->block_dev.private_data = dev;
        dev->block_dev.next = NULL;
        
        // Register with block device system
        extern error_code_t block_device_register(block_device_t* device);
        block_device_register(&dev->block_dev);
        
        ata_device_count++;
        kinfo("ATA device %u: %s (%llu sectors)\n", ata_device_count - 1, dev->model, dev->sectors);
    }
    
    // Check primary channel, slave
    if (ata_device_count < MAX_ATA_DEVICES) {
        dev = &ata_devices[ata_device_count];
        dev->base_port = ATA_PRIMARY_DATA;
        dev->control_port = ATA_PRIMARY_CONTROL;
        dev->drive = ATA_DRIVE_SLAVE;
        
        err = ata_identify(dev);
        if (err == ERR_OK) {
            dev->present = true;
            dev->block_dev.name = "hdb";  // Primary slave
            dev->block_dev.block_size = dev->sector_size;
            dev->block_dev.block_count = dev->sectors;
            dev->block_dev.read_block = ata_block_read;
            dev->block_dev.write_block = ata_block_write;
            dev->block_dev.read_blocks = ata_block_read_blocks;
            dev->block_dev.write_blocks = ata_block_write_blocks;
            dev->block_dev.private_data = dev;
            dev->block_dev.next = NULL;
            
            // Register with block device system
            extern error_code_t block_device_register(block_device_t* device);
            block_device_register(&dev->block_dev);
            
            ata_device_count++;
            kinfo("ATA device %u: %s (%llu sectors)\n", ata_device_count - 1, dev->model, dev->sectors);
        }
    }
    
    kinfo("ATA detection complete: %u device(s) found\n", ata_device_count);
    return ERR_OK;
}

/**
 * Get ATA device by index
 */
ata_device_t* ata_get_device(uint32_t index) {
    if (index >= ata_device_count) {
        return NULL;
    }
    return &ata_devices[index];
}

/**
 * Read sectors from ATA device
 */
error_code_t ata_read_sectors(ata_device_t* device, uint64_t lba, uint32_t count, void* buffer) {
    if (!device || !device->present) {
        return ERR_INVALID_ARG;
    }
    
    return ata_block_read_blocks(&device->block_dev, lba, count, buffer);
}

/**
 * Write sectors to ATA device
 */
error_code_t ata_write_sectors(ata_device_t* device, uint64_t lba, uint32_t count, const void* buffer) {
    if (!device || !device->present) {
        return ERR_INVALID_ARG;
    }
    
    return ata_block_write_blocks(&device->block_dev, lba, count, buffer);
}

