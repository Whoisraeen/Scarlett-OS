/**
 * @file fat32_create.c
 * @brief FAT32 file creation and deletion
 */

#include "../include/types.h"
#include "../include/fs/fat32.h"
#include "../include/fs/block.h"
#include "../include/mm/heap.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/errors.h"

/**
 * Convert filename to 8.3 format
 */
static void fat32_format_filename(const char* name, char* formatted) {
    memset(formatted, ' ', 11);
    
    // Find extension
    const char* dot = strchr(name, '.');
    const char* ext = dot ? (dot + 1) : NULL;
    size_t name_len = dot ? (dot - name) : strlen(name);
    size_t ext_len = ext ? strlen(ext) : 0;
    
    // Copy name (up to 8 characters)
    size_t i;
    for (i = 0; i < name_len && i < 8; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';  // Convert to uppercase
        }
        formatted[i] = c;
    }
    
    // Copy extension (up to 3 characters)
    if (ext && ext_len > 0) {
        for (i = 0; i < ext_len && i < 3; i++) {
            char c = ext[i];
            if (c >= 'a' && c <= 'z') {
                c = c - 'a' + 'A';  // Convert to uppercase
            }
            formatted[8 + i] = c;
        }
    }
}

/**
 * Find free directory entry slot
 */
error_code_t fat32_find_free_dir_entry(fat32_fs_t* fs, uint32_t cluster, 
                                               uint32_t* sector_out, uint32_t* entry_out) {
    uint8_t* cluster_data = (uint8_t*)kmalloc(fs->bytes_per_cluster);
    if (!cluster_data) {
        return ERR_OUT_OF_MEMORY;
    }
    
    uint32_t current_cluster = cluster;
    
    while (current_cluster >= 2 && current_cluster < FAT32_CLUSTER_EOF_MIN) {
        error_code_t err = fat32_read_cluster(fs, current_cluster, cluster_data);
        if (err != ERR_OK) {
            kfree(cluster_data);
            return err;
        }
        
        fat32_dir_entry_t* entries = (fat32_dir_entry_t*)cluster_data;
        size_t entries_per_cluster = fs->bytes_per_cluster / sizeof(fat32_dir_entry_t);
        
        // Search for free or deleted entry
        for (size_t i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                // Free or deleted entry
                *sector_out = fs->data_start_sector + ((current_cluster - 2) * fs->sectors_per_cluster);
                *entry_out = i;
                kfree(cluster_data);
                return ERR_OK;
            }
        }
        
        // Move to next cluster
        current_cluster = fat32_get_next_cluster(fs, current_cluster);
    }
    
    kfree(cluster_data);
    return ERR_DISK_FULL;  // No free entries
}

/**
 * Create a new file in FAT32
 */
error_code_t fat32_create_file(fat32_fs_t* fs, const char* path, fat32_dir_entry_t* entry) {
    if (!fs || !path || !entry) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("FAT32: Creating file %s\n", path);
    
    // Format filename to 8.3
    char formatted_name[11];
    fat32_format_filename(path, formatted_name);
    
    // Find free directory entry in root directory
    uint32_t sector;
    uint32_t entry_index;
    error_code_t err = fat32_find_free_dir_entry(fs, fs->root_cluster, &sector, &entry_index);
    if (err != ERR_OK) {
        return err;
    }
    
    // Allocate first cluster for file
    uint32_t first_cluster = fat32_alloc_cluster(fs);
    if (first_cluster < 2) {
        return ERR_DISK_FULL;
    }
    
    // Read directory sector
    uint8_t sector_data[512];
    err = block_device_read(fs->device, sector, sector_data);
    if (err != ERR_OK) {
        fat32_free_cluster(fs, first_cluster);
        return err;
    }
    
    // Create directory entry
    fat32_dir_entry_t* dir_entry = (fat32_dir_entry_t*)sector_data;
    dir_entry += entry_index;
    
    memcpy(dir_entry->name, formatted_name, 11);
    dir_entry->attributes = FAT32_ATTR_ARCHIVE;
    dir_entry->cluster_high = (first_cluster >> 16) & 0xFFFF;
    dir_entry->cluster_low = first_cluster & 0xFFFF;
    dir_entry->file_size = 0;
    
    // Set timestamps (simplified - would use real time)
    dir_entry->creation_time = 0;
    dir_entry->creation_date = 0;
    dir_entry->modification_time = 0;
    dir_entry->modification_date = 0;
    dir_entry->access_date = 0;
    
    // Write back directory sector
    err = block_device_write(fs->device, sector, sector_data);
    if (err != ERR_OK) {
        fat32_free_cluster(fs, first_cluster);
        return err;
    }
    
    // Copy entry to output
    memcpy(entry, dir_entry, sizeof(fat32_dir_entry_t));
    
    kinfo("FAT32: File created successfully (cluster %u)\n", first_cluster);
    return ERR_OK;
}

/**
 * Delete a file from FAT32
 */
error_code_t fat32_delete_file(fat32_fs_t* fs, const char* path) {
    if (!fs || !path) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("FAT32: Deleting file %s\n", path);
    
    // Find file
    fat32_dir_entry_t entry;
    error_code_t err = fat32_find_file(fs, path, &entry);
    if (err != ERR_OK) {
        return err;
    }
    
    // Get first cluster
    uint32_t first_cluster = (entry.cluster_low) | ((uint32_t)entry.cluster_high << 16);
    
    // Free cluster chain
    if (first_cluster >= 2) {
        fat32_free_cluster(fs, first_cluster);
    }
    
    // Mark directory entry as deleted (0xE5)
    // Find directory entry location
    extern error_code_t fat32_find_in_dir_location(fat32_fs_t* fs, uint32_t cluster, const char* name,
                                                    uint32_t* out_cluster, uint32_t* out_entry_index);
    
    // Parse path to get parent directory and filename
    char components[32][12];
    uint32_t component_count = 0;
    error_code_t err = fat32_parse_path(path, components, &component_count);
    if (err != ERR_OK || component_count == 0) {
        return err;
    }
    
    // Get parent directory cluster
    uint32_t parent_cluster = fs->root_cluster;
    for (uint32_t i = 0; i < component_count - 1; i++) {
        fat32_dir_entry_t dir_entry;
        err = fat32_find_in_dir(fs, parent_cluster, components[i], &dir_entry);
        if (err != ERR_OK) {
            return err;
        }
        parent_cluster = dir_entry.cluster_low | ((uint32_t)dir_entry.cluster_high << 16);
    }
    
    // Find entry location in parent directory
    uint32_t entry_cluster, entry_index;
    err = fat32_find_in_dir_location(fs, parent_cluster, components[component_count - 1], 
                                     &entry_cluster, &entry_index);
    if (err != ERR_OK) {
        return err;
    }
    
    // Read cluster containing the entry
    uint8_t* cluster_data = (uint8_t*)kmalloc(fs->bytes_per_cluster);
    if (!cluster_data) {
        return ERR_OUT_OF_MEMORY;
    }
    
    err = fat32_read_cluster(fs, entry_cluster, cluster_data);
    if (err != ERR_OK) {
        kfree(cluster_data);
        return err;
    }
    
    // Mark entry as deleted (0xE5)
    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)cluster_data;
    entries[entry_index].name[0] = 0xE5;
    
    // Write back cluster
    err = fat32_write_cluster(fs, entry_cluster, cluster_data);
    kfree(cluster_data);
    if (err != ERR_OK) {
        return err;
    }
    
    kinfo("FAT32: File deleted successfully\n");
    return ERR_OK;
}

