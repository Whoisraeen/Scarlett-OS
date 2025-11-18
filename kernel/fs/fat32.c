/**
 * @file fat32.c
 * @brief FAT32 filesystem implementation
 */

#include "../include/types.h"
#include "../include/fs/fat32.h"
#include "../include/fs/block.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/mm/heap.h"

/**
 * Read FAT32 boot sector
 */
static error_code_t fat32_read_boot_sector(block_device_t* device, fat32_boot_sector_t* boot) {
    if (!device || !boot) {
        return ERR_INVALID_ARG;
    }
    
    // Read first sector
    error_code_t err = block_device_read(device, 0, boot);
    if (err != ERR_OK) {
        return err;
    }
    
    // Verify boot signature
    if (boot->boot_signature_end != 0xAA55) {
        kerror("FAT32: Invalid boot signature\n");
        return ERR_INVALID_ELF;  // Reuse error code
    }
    
    // Verify FS type
    if (strncmp((char*)boot->fs_type, "FAT32", 5) != 0) {
        kerror("FAT32: Not a FAT32 filesystem\n");
        return ERR_INVALID_ELF;
    }
    
    return ERR_OK;
}

/**
 * Initialize FAT32 filesystem
 */
error_code_t fat32_init(block_device_t* device, fat32_fs_t* fs) {
    if (!device || !fs) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("Initializing FAT32 filesystem on device %s...\n", device->name);
    
    // Read boot sector
    error_code_t err = fat32_read_boot_sector(device, &fs->boot_sector);
    if (err != ERR_OK) {
        return err;
    }
    
    // Calculate filesystem parameters
    fs->device = device;
    fs->sectors_per_cluster = fs->boot_sector.sectors_per_cluster;
    fs->bytes_per_cluster = fs->sectors_per_cluster * fs->boot_sector.bytes_per_sector;
    
    // Calculate FAT location
    fs->fat_start_sector = fs->boot_sector.reserved_sectors;
    fs->fat_size_sectors = fs->boot_sector.sectors_per_fat_32;
    
    // Calculate data area
    fs->data_start_sector = fs->fat_start_sector + (fs->boot_sector.num_fats * fs->fat_size_sectors);
    fs->root_cluster = fs->boot_sector.root_cluster;
    
    // Calculate total clusters
    uint32_t data_sectors = fs->boot_sector.total_sectors_32 - fs->data_start_sector;
    fs->total_clusters = data_sectors / fs->sectors_per_cluster;
    
    // Allocate FAT cache
    fs->fat_cache = (uint8_t*)kmalloc(fs->boot_sector.bytes_per_sector);
    if (!fs->fat_cache) {
        return ERR_OUT_OF_MEMORY;
    }
    fs->fat_cache_sector = 0xFFFFFFFF;  // Invalid
    
    kinfo("FAT32: Sectors per cluster: %u, Bytes per cluster: %u\n",
          fs->sectors_per_cluster, fs->bytes_per_cluster);
    kinfo("FAT32: FAT start: %u, FAT size: %u sectors\n",
          fs->fat_start_sector, fs->fat_size_sectors);
    kinfo("FAT32: Data start: %u, Root cluster: %u\n",
          fs->data_start_sector, fs->root_cluster);
    kinfo("FAT32: Total clusters: %u\n", fs->total_clusters);
    
    return ERR_OK;
}

/**
 * Mount FAT32 filesystem
 */
error_code_t fat32_mount(fat32_fs_t* fs, const char* mountpoint) {
    (void)mountpoint;  // For now, just initialize
    kinfo("FAT32: Mounted at %s\n", mountpoint);
    return ERR_OK;
}

/**
 * Read a cluster from the filesystem
 */
error_code_t fat32_read_cluster(fat32_fs_t* fs, uint32_t cluster, void* buffer) {
    if (!fs || !buffer || cluster < 2 || cluster >= fs->total_clusters + 2) {
        return ERR_INVALID_ARG;
    }
    
    // Calculate sector number
    uint32_t first_sector = fs->data_start_sector + ((cluster - 2) * fs->sectors_per_cluster);
    
    // Read cluster
    return block_device_read_blocks(fs->device, first_sector, fs->sectors_per_cluster, buffer);
}

/**
 * Write a cluster to the filesystem
 */
error_code_t fat32_write_cluster(fat32_fs_t* fs, uint32_t cluster, const void* buffer) {
    if (!fs || !buffer || cluster < 2 || cluster >= fs->total_clusters + 2) {
        return ERR_INVALID_ARG;
    }
    
    // Calculate sector number
    uint32_t first_sector = fs->data_start_sector + ((cluster - 2) * fs->sectors_per_cluster);
    
    // Write cluster
    return block_device_write_blocks(fs->device, first_sector, fs->sectors_per_cluster, buffer);
}

/**
 * Get next cluster in chain
 */
uint32_t fat32_get_next_cluster(fat32_fs_t* fs, uint32_t cluster) {
    if (!fs || cluster < 2 || cluster >= fs->total_clusters + 2) {
        return FAT32_CLUSTER_EOF_MIN;
    }
    
    // Calculate FAT entry location
    uint32_t fat_offset = cluster * 4;  // 4 bytes per FAT32 entry
    uint32_t fat_sector = fs->fat_start_sector + (fat_offset / fs->boot_sector.bytes_per_sector);
    uint32_t fat_entry_offset = fat_offset % fs->boot_sector.bytes_per_sector;
    
    // Read FAT sector if not cached
    if (fat_sector != fs->fat_cache_sector) {
        error_code_t err = block_device_read(fs->device, fat_sector, fs->fat_cache);
        if (err != ERR_OK) {
            return FAT32_CLUSTER_EOF_MIN;
        }
        fs->fat_cache_sector = fat_sector;
    }
    
    // Read FAT entry
    uint32_t* fat_entry = (uint32_t*)(fs->fat_cache + fat_entry_offset);
    uint32_t next_cluster = *fat_entry & 0x0FFFFFFF;  // Mask upper 4 bits
    
    return next_cluster;
}

/**
 * Set next cluster in chain
 */
error_code_t fat32_set_next_cluster(fat32_fs_t* fs, uint32_t cluster, uint32_t next) {
    if (!fs || cluster < 2 || cluster >= fs->total_clusters + 2) {
        return ERR_INVALID_ARG;
    }
    
    // Calculate FAT entry location
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs->fat_start_sector + (fat_offset / fs->boot_sector.bytes_per_sector);
    uint32_t fat_entry_offset = fat_offset % fs->boot_sector.bytes_per_sector;
    
    // Read FAT sector if not cached
    if (fat_sector != fs->fat_cache_sector) {
        error_code_t err = block_device_read(fs->device, fat_sector, fs->fat_cache);
        if (err != ERR_OK) {
            return err;
        }
        fs->fat_cache_sector = fat_sector;
    }
    
    // Update FAT entry
    uint32_t* fat_entry = (uint32_t*)(fs->fat_cache + fat_entry_offset);
    *fat_entry = (*fat_entry & 0xF0000000) | (next & 0x0FFFFFFF);
    
    // Write back FAT sector
    error_code_t err = block_device_write(fs->device, fat_sector, fs->fat_cache);
    if (err != ERR_OK) {
        return err;
    }
    
    // Update all FAT copies
    for (uint8_t i = 1; i < fs->boot_sector.num_fats; i++) {
        uint32_t fat_copy_sector = fat_sector + (i * fs->fat_size_sectors);
        block_device_write(fs->device, fat_copy_sector, fs->fat_cache);
    }
    
    return ERR_OK;
}

/**
 * Allocate a free cluster
 */
uint32_t fat32_alloc_cluster(fat32_fs_t* fs) {
    if (!fs) {
        return 0;
    }
    
    // Search for free cluster (simple linear search)
    for (uint32_t cluster = 2; cluster < fs->total_clusters + 2; cluster++) {
        uint32_t next = fat32_get_next_cluster(fs, cluster);
        if (next == FAT32_CLUSTER_FREE) {
            // Mark as EOF
            fat32_set_next_cluster(fs, cluster, FAT32_CLUSTER_EOF_MIN);
            return cluster;
        }
    }
    
    return 0;  // No free clusters
}

/**
 * Free a cluster
 */
error_code_t fat32_free_cluster(fat32_fs_t* fs, uint32_t cluster) {
    if (!fs || cluster < 2) {
        return ERR_INVALID_ARG;
    }
    
    // Free cluster chain
    while (cluster >= 2 && cluster < fs->total_clusters + 2) {
        uint32_t next = fat32_get_next_cluster(fs, cluster);
        fat32_set_next_cluster(fs, cluster, FAT32_CLUSTER_FREE);
        cluster = next;
        
        if (next >= FAT32_CLUSTER_EOF_MIN) {
            break;
        }
    }
    
    return ERR_OK;
}

/**
 * Read directory entries from cluster
 */
error_code_t fat32_read_dir(fat32_fs_t* fs, uint32_t cluster, fat32_dir_entry_t* entries, size_t max_entries) {
    if (!fs || !entries) {
        return ERR_INVALID_ARG;
    }
    
    // Read cluster
    uint8_t* cluster_data = (uint8_t*)kmalloc(fs->bytes_per_cluster);
    if (!cluster_data) {
        return ERR_OUT_OF_MEMORY;
    }
    
    error_code_t err = fat32_read_cluster(fs, cluster, cluster_data);
    if (err != ERR_OK) {
        kfree(cluster_data);
        return err;
    }
    
    // Parse directory entries
    size_t entry_count = 0;
    fat32_dir_entry_t* dir_entries = (fat32_dir_entry_t*)cluster_data;
    size_t entries_per_cluster = fs->bytes_per_cluster / sizeof(fat32_dir_entry_t);
    
    for (size_t i = 0; i < entries_per_cluster && entry_count < max_entries; i++) {
        // Check if entry is valid (not free)
        if (dir_entries[i].name[0] == 0x00) {
            break;  // End of directory
        }
        if (dir_entries[i].name[0] == 0xE5) {
            continue;  // Deleted entry
        }
        
        // Copy entry
        memcpy(&entries[entry_count], &dir_entries[i], sizeof(fat32_dir_entry_t));
        entry_count++;
    }
    
    kfree(cluster_data);
    return ERR_OK;
}

/**
 * Find file in directory (simplified)
 */
error_code_t fat32_find_file(fat32_fs_t* fs, const char* path, fat32_dir_entry_t* entry) {
    if (!fs || !path || !entry) {
        return ERR_INVALID_ARG;
    }
    
    // For now, just search root directory
    // TODO: Implement path traversal
    
    fat32_dir_entry_t entries[16];
    error_code_t err = fat32_read_dir(fs, fs->root_cluster, entries, 16);
    if (err != ERR_OK) {
        return err;
    }
    
    // Simple filename matching (8.3 format)
    // TODO: Implement proper path parsing
    
    return ERR_NOT_FOUND;
}

