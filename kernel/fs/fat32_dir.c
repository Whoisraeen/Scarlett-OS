/**
 * @file fat32_dir.c
 * @brief FAT32 directory operations
 */

#include "../include/types.h"
#include "../include/fs/fat32.h"
#include "../include/fs/block.h"
#include "../include/mm/heap.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/errors.h"

// Forward declaration
extern error_code_t fat32_find_free_dir_entry(fat32_fs_t* fs, uint32_t cluster, 
                                               uint32_t* sector_out, uint32_t* entry_out);
extern error_code_t fat32_parse_path(const char* path, char components[][12], uint32_t* component_count);
extern error_code_t fat32_find_in_dir(fat32_fs_t* fs, uint32_t cluster, const char* name, fat32_dir_entry_t* entry);

// Directory handle structure
typedef struct {
    fat32_fs_t* fs;
    uint32_t cluster;
    uint32_t current_entry;
    uint8_t* cluster_buffer;
    bool cluster_dirty;
} fat32_dir_handle_t;

// Maximum directory handles
#define MAX_DIR_HANDLES 16
static fat32_dir_handle_t dir_handles[MAX_DIR_HANDLES];
static uint32_t next_dir_handle = 0;

/**
 * Allocate directory handle
 */
static fd_t fat32_alloc_dir_handle(void) {
    for (uint32_t i = 0; i < MAX_DIR_HANDLES; i++) {
        uint32_t idx = (next_dir_handle + i) % MAX_DIR_HANDLES;
        if (dir_handles[idx].fs == NULL) {
            next_dir_handle = (idx + 1) % MAX_DIR_HANDLES;
            return idx;
        }
    }
    return -1;
}

/**
 * Free directory handle
 */
static void fat32_free_dir_handle(fd_t fd) {
    if (fd >= 0 && fd < MAX_DIR_HANDLES) {
        if (dir_handles[fd].cluster_buffer) {
            kfree(dir_handles[fd].cluster_buffer);
        }
        memset(&dir_handles[fd], 0, sizeof(fat32_dir_handle_t));
    }
}

/**
 * Get directory handle
 */
static fat32_dir_handle_t* fat32_get_dir_handle(fd_t fd) {
    if (fd < 0 || fd >= MAX_DIR_HANDLES || dir_handles[fd].fs == NULL) {
        return NULL;
    }
    return &dir_handles[fd];
}

/**
 * FAT32 mkdir implementation
 */
error_code_t fat32_mkdir(fat32_fs_t* fs, const char* path) {
    if (!fs || !path) {
        return ERR_INVALID_ARG;
    }
    
    // Parse path
    char components[32][12];
    uint32_t component_count;
    error_code_t err = fat32_parse_path(path, components, &component_count);
    if (err != ERR_OK) {
        return err;
    }
    
    if (component_count == 0) {
        return ERR_INVALID_ARG;  // Cannot create root
    }
    
    // Get parent directory and new directory name
    const char* dirname = components[component_count - 1];
    uint32_t parent_cluster = fs->root_cluster;
    
    // Traverse to parent directory
    for (uint32_t i = 0; i < component_count - 1; i++) {
        fat32_dir_entry_t entry;
        err = fat32_find_in_dir(fs, parent_cluster, components[i], &entry);
        if (err != ERR_OK) {
            return err;
        }
        if (!(entry.attributes & FAT32_ATTR_DIRECTORY)) {
            return ERR_NOT_DIRECTORY;
        }
        parent_cluster = entry.cluster_low | ((uint32_t)entry.cluster_high << 16);
    }
    
    // Check if directory already exists
    fat32_dir_entry_t existing;
    if (fat32_find_in_dir(fs, parent_cluster, dirname, &existing) == ERR_OK) {
        return ERR_ALREADY_EXISTS;
    }
    
    // Allocate cluster for new directory
    uint32_t new_cluster = fat32_alloc_cluster(fs);
    if (new_cluster < 2) {
        return ERR_DISK_FULL;
    }
    
    // Initialize directory with . and .. entries
    uint8_t* cluster_data = (uint8_t*)kmalloc(fs->bytes_per_cluster);
    if (!cluster_data) {
        fat32_free_cluster(fs, new_cluster);
        return ERR_OUT_OF_MEMORY;
    }
    
    memset(cluster_data, 0, fs->bytes_per_cluster);
    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)cluster_data;
    
    // Create . entry
    memset(entries[0].name, ' ', 11);
    entries[0].name[0] = '.';
    entries[0].attributes = FAT32_ATTR_DIRECTORY;
    entries[0].cluster_high = (new_cluster >> 16) & 0xFFFF;
    entries[0].cluster_low = new_cluster & 0xFFFF;
    entries[0].file_size = 0;
    
    // Create .. entry
    memset(entries[1].name, ' ', 11);
    entries[1].name[0] = '.';
    entries[1].name[1] = '.';
    entries[1].attributes = FAT32_ATTR_DIRECTORY;
    entries[1].cluster_high = (parent_cluster >> 16) & 0xFFFF;
    entries[1].cluster_low = parent_cluster & 0xFFFF;
    entries[1].file_size = 0;
    
    // Write cluster
    err = fat32_write_cluster(fs, new_cluster, cluster_data);
    if (err != ERR_OK) {
        kfree(cluster_data);
        fat32_free_cluster(fs, new_cluster);
        return err;
    }
    
    kfree(cluster_data);
    
    // Find free entry in parent directory
    uint32_t sector;
    uint32_t entry_index;
    err = fat32_find_free_dir_entry(fs, parent_cluster, &sector, &entry_index);
    if (err != ERR_OK) {
        fat32_free_cluster(fs, new_cluster);
        return err;
    }
    
    // Read parent directory sector
    uint8_t sector_data[512];
    err = block_device_read(fs->device, sector, sector_data);
    if (err != ERR_OK) {
        fat32_free_cluster(fs, new_cluster);
        return err;
    }
    
    // Create directory entry
    fat32_dir_entry_t* dir_entry = (fat32_dir_entry_t*)sector_data;
    dir_entry += entry_index;
    
    // Format directory name
    char formatted[11];
    memset(formatted, ' ', 11);
    size_t name_len = strlen(dirname);
    for (size_t i = 0; i < name_len && i < 11; i++) {
        char c = dirname[i];
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
        formatted[i] = c;
    }
    
    memcpy(dir_entry->name, formatted, 11);
    dir_entry->attributes = FAT32_ATTR_DIRECTORY;
    dir_entry->cluster_high = (new_cluster >> 16) & 0xFFFF;
    dir_entry->cluster_low = new_cluster & 0xFFFF;
    dir_entry->file_size = 0;
    
    // Write back parent directory
    err = block_device_write(fs->device, sector, sector_data);
    if (err != ERR_OK) {
        fat32_free_cluster(fs, new_cluster);
        return err;
    }
    
    kinfo("FAT32: Created directory %s (cluster %u)\n", dirname, new_cluster);
    return ERR_OK;
}

/**
 * FAT32 rmdir implementation
 */
error_code_t fat32_rmdir(fat32_fs_t* fs, const char* path) {
    if (!fs || !path) {
        return ERR_INVALID_ARG;
    }
    
    // TODO: Implement directory removal
    // 1. Find directory
    // 2. Check if directory is empty
    // 3. Remove directory entry from parent
    // 4. Free directory clusters
    
    kinfo("FAT32: rmdir %s (not yet implemented)\n", path);
    return ERR_NOT_SUPPORTED;
}

/**
 * FAT32 opendir implementation
 */
error_code_t fat32_opendir(fat32_fs_t* fs, const char* path, fd_t* fd) {
    if (!fs || !path || !fd) {
        return ERR_INVALID_ARG;
    }
    
    // Find directory
    fat32_dir_entry_t entry;
    error_code_t err = fat32_find_file(fs, path, &entry);
    if (err != ERR_OK) {
        return err;
    }
    
    // Check if it's a directory
    if (!(entry.attributes & FAT32_ATTR_DIRECTORY)) {
        return ERR_NOT_DIRECTORY;
    }
    
    // Allocate directory handle
    fd_t handle = fat32_alloc_dir_handle();
    if (handle < 0) {
        return ERR_OUT_OF_MEMORY;
    }
    
    fat32_dir_handle_t* dir = &dir_handles[handle];
    dir->fs = fs;
    dir->cluster = entry.cluster_low | ((uint32_t)entry.cluster_high << 16);
    dir->current_entry = 0;
    dir->cluster_buffer = (uint8_t*)kmalloc(fs->bytes_per_cluster);
    if (!dir->cluster_buffer) {
        fat32_free_dir_handle(handle);
        return ERR_OUT_OF_MEMORY;
    }
    dir->cluster_dirty = false;
    
    // Load first cluster
    err = fat32_read_cluster(fs, dir->cluster, dir->cluster_buffer);
    if (err != ERR_OK) {
        fat32_free_dir_handle(handle);
        return err;
    }
    
    *fd = handle;
    return ERR_OK;
}

/**
 * FAT32 readdir implementation
 */
error_code_t fat32_readdir(fat32_fs_t* fs, fd_t fd, vfs_dirent_t* entry) {
    if (!fs || !entry) {
        return ERR_INVALID_ARG;
    }
    
    fat32_dir_handle_t* dir = fat32_get_dir_handle(fd);
    if (!dir || dir->fs != fs) {
        return ERR_INVALID_ARG;
    }
    
    // Read entries from current cluster
    fat32_dir_entry_t* fat_entry = (fat32_dir_entry_t*)dir->cluster_buffer;
    uint32_t entries_per_cluster = fs->bytes_per_cluster / sizeof(fat32_dir_entry_t);
    
    while (1) {
        // Check if we need to load a new cluster
        if (dir->current_entry >= entries_per_cluster) {
            // Get next cluster
            uint32_t next_cluster = fat32_get_next_cluster(fs, dir->cluster);
            if (next_cluster >= FAT32_CLUSTER_EOF_MIN) {
                // End of directory
                return ERR_END_OF_FILE;
            }
            
            dir->cluster = next_cluster;
            dir->current_entry = 0;
            
            // Write back current cluster if dirty
            if (dir->cluster_dirty) {
                error_code_t err = fat32_write_cluster(fs, dir->cluster, dir->cluster_buffer);
                if (err != ERR_OK) {
                    return err;
                }
                dir->cluster_dirty = false;
            }
            
            // Load new cluster
            error_code_t err = fat32_read_cluster(fs, dir->cluster, dir->cluster_buffer);
            if (err != ERR_OK) {
                return err;
            }
            
            fat_entry = (fat32_dir_entry_t*)dir->cluster_buffer;
        }
        
        // Check if entry is valid
        if (fat_entry[dir->current_entry].name[0] == 0) {
            // End of directory
            return ERR_END_OF_FILE;
        }
        
        // Skip deleted entries and long name entries
        if (fat_entry[dir->current_entry].name[0] != 0xE5 &&
            fat_entry[dir->current_entry].name[0] != 0x00 &&
            (fat_entry[dir->current_entry].attributes & FAT32_ATTR_LONG_NAME) == 0) {
            
            // Convert 8.3 name to regular name
            char name[13];
            size_t name_idx = 0;
            
            // Copy base name
            for (int i = 0; i < 8; i++) {
                char c = fat_entry[dir->current_entry].name[i];
                if (c == ' ') break;
                name[name_idx++] = c;
            }
            
            // Add extension if present
            if (fat_entry[dir->current_entry].name[8] != ' ') {
                name[name_idx++] = '.';
                for (int i = 8; i < 11; i++) {
                    char c = fat_entry[dir->current_entry].name[i];
                    if (c == ' ') break;
                    name[name_idx++] = c;
                }
            }
            name[name_idx] = '\0';
            
            // Fill entry
            entry->ino = fat_entry[dir->current_entry].cluster_low |
                        ((uint64_t)fat_entry[dir->current_entry].cluster_high << 16);
            strncpy(entry->name, name, sizeof(entry->name) - 1);
            entry->name[sizeof(entry->name) - 1] = '\0';
            
            if (fat_entry[dir->current_entry].attributes & FAT32_ATTR_DIRECTORY) {
                entry->type = VFS_TYPE_DIRECTORY;
            } else {
                entry->type = VFS_TYPE_FILE;
            }
            
            dir->current_entry++;
            return ERR_OK;
        }
        
        dir->current_entry++;
    }
}

/**
 * FAT32 closedir implementation
 */
error_code_t fat32_closedir(fat32_fs_t* fs, fd_t fd) {
    if (!fs) {
        return ERR_INVALID_ARG;
    }
    
    fat32_dir_handle_t* dir = fat32_get_dir_handle(fd);
    if (!dir || dir->fs != fs) {
        return ERR_INVALID_ARG;
    }
    
    // Write back cluster if dirty
    if (dir->cluster_dirty) {
        fat32_write_cluster(fs, dir->cluster, dir->cluster_buffer);
    }
    
    fat32_free_dir_handle(fd);
    return ERR_OK;
}

