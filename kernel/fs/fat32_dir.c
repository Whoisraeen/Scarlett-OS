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
    
    // TODO: Implement directory creation
    // 1. Parse path to get parent directory and new directory name
    // 2. Find parent directory
    // 3. Allocate cluster for new directory
    // 4. Create directory entry in parent
    // 5. Initialize new directory with . and .. entries
    
    kinfo("FAT32: mkdir %s (not yet implemented)\n", path);
    return ERR_NOT_SUPPORTED;
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

