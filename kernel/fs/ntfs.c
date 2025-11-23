/**
 * @file ntfs.c
 * @brief NTFS filesystem implementation (read-only)
 */

#include "../include/fs/ntfs.h"
#include "../include/fs/block.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/mm/heap.h"

/**
 * Read NTFS boot sector
 */
static error_code_t ntfs_read_boot_sector(block_device_t* device, ntfs_boot_sector_t* boot) {
    if (!device || !boot) {
        return ERR_INVALID_ARG;
    }
    
    // Read first sector
    error_code_t err = block_device_read(device, 0, boot);
    if (err != ERR_OK) {
        return err;
    }
    
    // Verify magic signature
    if (memcmp(boot->oem_id, NTFS_BOOT_SECTOR_MAGIC, 8) != 0) {
        kerror("NTFS: Invalid boot sector signature\n");
        return ERR_INVALID_ELF;
    }
    
    // Verify boot signature
    if (boot->boot_signature != 0xAA55) {
        kerror("NTFS: Invalid boot signature\n");
        return ERR_INVALID_ELF;
    }
    
    return ERR_OK;
}

/**
 * Initialize NTFS filesystem
 */
error_code_t ntfs_init(block_device_t* device, ntfs_fs_t* fs) {
    if (!device || !fs) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("Initializing NTFS filesystem on device %s...\n", device->name);
    
    // Read boot sector
    error_code_t err = ntfs_read_boot_sector(device, &fs->boot_sector);
    if (err != ERR_OK) {
        return err;
    }
    
    // Calculate filesystem parameters
    fs->device = device;
    fs->bytes_per_sector = fs->boot_sector.bytes_per_sector;
    fs->bytes_per_cluster = fs->bytes_per_sector * fs->boot_sector.sectors_per_cluster;
    fs->mft_cluster = fs->boot_sector.mft_cluster;
    fs->mft_mirror_cluster = fs->boot_sector.mft_mirror_cluster;
    
    // Calculate MFT record size
    if (fs->boot_sector.clusters_per_mft_record < 0) {
        // Negative value means size in bytes (2^abs(value))
        fs->mft_record_size = 1 << (-fs->boot_sector.clusters_per_mft_record);
    } else {
        fs->mft_record_size = fs->boot_sector.clusters_per_mft_record * fs->bytes_per_cluster;
    }
    
    fs->initialized = true;
    
    kinfo("NTFS: Sector size: %u, Cluster size: %u, MFT record size: %u\n",
          fs->bytes_per_sector, fs->bytes_per_cluster, fs->mft_record_size);
    kinfo("NTFS: MFT cluster: %llu, Mirror cluster: %llu\n",
          fs->mft_cluster, fs->mft_mirror_cluster);
    
    return ERR_OK;
}

/**
 * Mount NTFS filesystem
 */
error_code_t ntfs_mount(ntfs_fs_t* fs, const char* mountpoint) {
    if (!fs || !fs->initialized) {
        return ERR_INVALID_STATE;
    }
    
    (void)mountpoint;
    kinfo("NTFS: Mounted at %s (read-only)\n", mountpoint);
    return ERR_OK;
}

/**
 * Unmount NTFS filesystem
 */
error_code_t ntfs_unmount(ntfs_fs_t* fs) {
    if (!fs) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("NTFS: Unmounting...\n");
    fs->initialized = false;
    return ERR_OK;
}

/**
 * Read MFT record
 */
error_code_t ntfs_read_mft_record(ntfs_fs_t* fs, uint64_t record_number, void* buffer) {
    if (!fs || !fs->initialized || !buffer) {
        return ERR_INVALID_ARG;
    }
    
    // Calculate cluster and sector
    uint64_t mft_cluster = fs->mft_cluster + (record_number * fs->mft_record_size) / fs->bytes_per_cluster;
    uint64_t mft_sector = mft_cluster * fs->boot_sector.sectors_per_cluster;
    
    // Read sectors containing MFT record
    uint32_t sectors_per_record = (fs->mft_record_size + fs->bytes_per_sector - 1) / fs->bytes_per_sector;
    
    uint8_t* sector_buffer = (uint8_t*)kmalloc(sectors_per_record * fs->bytes_per_sector);
    if (!sector_buffer) {
        return ERR_OUT_OF_MEMORY;
    }
    
    for (uint32_t i = 0; i < sectors_per_record; i++) {
        error_code_t err = block_device_read(fs->device, mft_sector + i, sector_buffer + (i * fs->bytes_per_sector));
        if (err != ERR_OK) {
            kfree(sector_buffer);
            return err;
        }
    }
    
    // Copy MFT record
    memcpy(buffer, sector_buffer, fs->mft_record_size);
    kfree(sector_buffer);
    
    // Verify magic
    ntfs_mft_record_t* record = (ntfs_mft_record_t*)buffer;
    if (record->magic != 0x454C4946) {  // "FILE"
        return ERR_INVALID_ELF;
    }
    
    return ERR_OK;
}

/**
 * Find file in NTFS (simplified - would need full path parsing)
 */
error_code_t ntfs_find_file(ntfs_fs_t* fs, const char* path, uint64_t* mft_record) {
    if (!fs || !fs->initialized || !path || !mft_record) {
        return ERR_INVALID_ARG;
    }
    
    // TODO: Implement full path resolution
    // For now, return root directory (MFT record 5)
    *mft_record = 5;
    return ERR_OK;
}

/**
 * Read file data from NTFS (simplified)
 */
error_code_t ntfs_read_file(ntfs_fs_t* fs, uint64_t mft_record, void* buffer, size_t offset, size_t count, size_t* bytes_read) {
    if (!fs || !fs->initialized || !buffer || !bytes_read) {
        return ERR_INVALID_ARG;
    }
    
    // Read MFT record
    uint8_t* mft_buffer = (uint8_t*)kmalloc(fs->mft_record_size);
    if (!mft_buffer) {
        return ERR_OUT_OF_MEMORY;
    }
    
    error_code_t err = ntfs_read_mft_record(fs, mft_record, mft_buffer);
    if (err != ERR_OK) {
        kfree(mft_buffer);
        return err;
    }
    
    // TODO: Parse attributes and read data runs
    // For now, return not implemented
    kfree(mft_buffer);
    *bytes_read = 0;
    return ERR_NOT_SUPPORTED;
}

