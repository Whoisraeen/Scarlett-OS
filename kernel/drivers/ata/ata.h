/**
 * @file ata.h
 * @brief ATA/IDE driver interface
 * 
 * Provides ATA (Advanced Technology Attachment) and IDE (Integrated Drive Electronics)
 * driver for accessing hard drives and optical drives.
 */

#ifndef KERNEL_DRIVERS_ATA_H
#define KERNEL_DRIVERS_ATA_H

#include "../../include/types.h"
#include "../../include/errors.h"
#include "../../include/fs/block.h"

// ATA I/O ports
#define ATA_PRIMARY_DATA        0x1F0
#define ATA_PRIMARY_ERROR       0x1F1
#define ATA_PRIMARY_SECTOR_COUNT 0x1F2
#define ATA_PRIMARY_LBA_LOW     0x1F3
#define ATA_PRIMARY_LBA_MID     0x1F4
#define ATA_PRIMARY_LBA_HIGH    0x1F5
#define ATA_PRIMARY_DRIVE       0x1F6
#define ATA_PRIMARY_COMMAND     0x1F7
#define ATA_PRIMARY_STATUS      0x1F7
#define ATA_PRIMARY_ALT_STATUS  0x3F6
#define ATA_PRIMARY_CONTROL     0x3F6

#define ATA_SECONDARY_DATA      0x170
#define ATA_SECONDARY_ERROR     0x171
#define ATA_SECONDARY_SECTOR_COUNT 0x172
#define ATA_SECONDARY_LBA_LOW   0x173
#define ATA_SECONDARY_LBA_MID   0x174
#define ATA_SECONDARY_LBA_HIGH  0x175
#define ATA_SECONDARY_DRIVE     0x176
#define ATA_SECONDARY_COMMAND   0x177
#define ATA_SECONDARY_STATUS    0x177
#define ATA_SECONDARY_ALT_STATUS 0x376
#define ATA_SECONDARY_CONTROL   0x376

// ATA drive select
#define ATA_DRIVE_MASTER        0xA0
#define ATA_DRIVE_SLAVE         0xB0

// ATA commands
#define ATA_CMD_IDENTIFY        0xEC
#define ATA_CMD_READ_SECTORS    0x20
#define ATA_CMD_READ_SECTORS_EXT 0x24
#define ATA_CMD_WRITE_SECTORS   0x30
#define ATA_CMD_WRITE_SECTORS_EXT 0x34
#define ATA_CMD_FLUSH_CACHE     0xE7
#define ATA_CMD_FLUSH_CACHE_EXT 0xEA

// ATA status register bits
#define ATA_STATUS_ERR          0x01
#define ATA_STATUS_IDX          0x02
#define ATA_STATUS_CORR         0x04
#define ATA_STATUS_DRQ          0x08  // Data Request
#define ATA_STATUS_SRV          0x10
#define ATA_STATUS_DF           0x20
#define ATA_STATUS_RDY          0x40  // Ready
#define ATA_STATUS_BSY          0x80  // Busy

// ATA error register bits
#define ATA_ERROR_AMNF          0x01
#define ATA_ERROR_TK0NF         0x02
#define ATA_ERROR_ABRT          0x04
#define ATA_ERROR_MCR           0x08
#define ATA_ERROR_IDNF          0x10
#define ATA_ERROR_MC            0x20
#define ATA_ERROR_UNC           0x40
#define ATA_ERROR_BBK           0x80

// ATA device structure
typedef struct {
    uint16_t base_port;         // Base I/O port (0x1F0 or 0x170)
    uint16_t control_port;      // Control port (0x3F6 or 0x376)
    uint8_t drive;              // Master (0xA0) or Slave (0xB0)
    bool present;               // Is drive present?
    bool lba48;                 // Supports 48-bit LBA?
    uint64_t sectors;           // Total number of sectors
    uint32_t sector_size;       // Sector size (usually 512)
    char model[41];             // Drive model string
    block_device_t block_dev;   // Block device interface
} ata_device_t;

// Maximum ATA devices
#define MAX_ATA_DEVICES 4

// ATA functions
error_code_t ata_init(void);
error_code_t ata_detect_devices(void);
ata_device_t* ata_get_device(uint32_t index);
error_code_t ata_read_sectors(ata_device_t* device, uint64_t lba, uint32_t count, void* buffer);
error_code_t ata_write_sectors(ata_device_t* device, uint64_t lba, uint32_t count, const void* buffer);

#endif // KERNEL_DRIVERS_ATA_H

