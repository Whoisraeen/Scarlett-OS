/**
 * @file fat32_file.c
 * @brief FAT32 file operations implementation
 * 
 * Implements file-level operations (open, read, write, close) for FAT32.
 */

#include "../include/types.h"
#include "../include/fs/fat32.h"
#include "../include/fs/vfs.h"
#include "../include/fs/block.h"
#include "../include/mm/heap.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/errors.h"

// Forward declarations
extern void kputc(char c);

// FAT32 file handle
typedef struct {
    fat32_fs_t* fs;              // Filesystem
    fat32_dir_entry_t entry;     // Directory entry
    uint32_t current_cluster;    // Current cluster in file
    uint64_t position;           // Current position in file
    uint64_t size;                // File size
    uint8_t* cluster_buffer;      // Current cluster buffer
    bool cluster_dirty;          // Has cluster been modified?
} fat32_file_t;

// Maximum open files per filesystem
#define MAX_FAT32_FILES 64
static fat32_file_t fat32_files[MAX_FAT32_FILES];
static bool fat32_file_slots[MAX_FAT32_FILES];

/**
 * Allocate a FAT32 file handle
 */
static fat32_file_t* fat32_alloc_file(void) {
    for (int i = 0; i < MAX_FAT32_FILES; i++) {
        if (!fat32_file_slots[i]) {
            fat32_file_slots[i] = true;
            memset(&fat32_files[i], 0, sizeof(fat32_file_t));
            return &fat32_files[i];
        }
    }
    return NULL;
}

/**
 * Free a FAT32 file handle
 */
static void fat32_free_file(fat32_file_t* file) {
    if (!file) return;
    
    // Write back dirty cluster
    if (file->cluster_dirty && file->cluster_buffer && file->current_cluster >= 2) {
        fat32_write_cluster(file->fs, file->current_cluster, file->cluster_buffer);
        file->cluster_dirty = false;
    }
    
    // Free cluster buffer
    if (file->cluster_buffer) {
        kfree(file->cluster_buffer);
        file->cluster_buffer = NULL;
    }
    
    // Mark slot as free
    for (int i = 0; i < MAX_FAT32_FILES; i++) {
        if (&fat32_files[i] == file) {
            fat32_file_slots[i] = false;
            break;
        }
    }
}

/**
 * Load cluster into buffer
 */
static error_code_t fat32_load_cluster(fat32_file_t* file, uint32_t cluster) {
    if (!file || cluster < 2) {
        return ERR_INVALID_ARG;
    }
    
    // Write back previous cluster if dirty
    if (file->cluster_dirty && file->cluster_buffer && file->current_cluster >= 2) {
        error_code_t err = fat32_write_cluster(file->fs, file->current_cluster, file->cluster_buffer);
        if (err != ERR_OK) {
            return err;
        }
        file->cluster_dirty = false;
    }
    
    // Allocate buffer if needed
    if (!file->cluster_buffer) {
        file->cluster_buffer = (uint8_t*)kmalloc(file->fs->bytes_per_cluster);
        if (!file->cluster_buffer) {
            return ERR_OUT_OF_MEMORY;
        }
    }
    
    // Read cluster
    error_code_t err = fat32_read_cluster(file->fs, cluster, file->cluster_buffer);
    if (err != ERR_OK) {
        return err;
    }
    
    file->current_cluster = cluster;
    return ERR_OK;
}

/**
 * Get cluster number for a given file position
 */
static uint32_t fat32_get_cluster_for_position(fat32_file_t* file, uint64_t position) {
    if (!file || position >= file->size) {
        return 0;
    }
    
    // Calculate which cluster this position is in
    uint32_t cluster_offset = position / file->fs->bytes_per_cluster;
    
    // Start from first cluster
    uint32_t cluster = (file->entry.cluster_low) | ((uint32_t)file->entry.cluster_high << 16);
    
    // Traverse cluster chain
    for (uint32_t i = 0; i < cluster_offset; i++) {
        cluster = fat32_get_next_cluster(file->fs, cluster);
        if (cluster >= FAT32_CLUSTER_EOF_MIN) {
            return 0;  // Invalid
        }
    }
    
    return cluster;
}

/**
 * FAT32 open file
 */
error_code_t fat32_file_open(fat32_fs_t* fs, const char* path, uint64_t flags, fd_t* fd) {
    if (!fs || !path || !fd) {
        return ERR_INVALID_ARG;
    }
    
    // Find file in directory
    fat32_dir_entry_t entry;
    error_code_t err = fat32_find_file(fs, path, &entry);
    if (err != ERR_OK) {
        // File not found - create if CREATE flag is set
        if (flags & VFS_MODE_CREATE) {
            // Create new file
            fat32_dir_entry_t new_entry;
            error_code_t create_err = fat32_create_file(fs, path, &new_entry);
            if (create_err != ERR_OK) {
                return create_err;
            }
            // Use the newly created entry
            entry = new_entry;
        } else {
            return ERR_NOT_FOUND;
        }
    }
    
    // Check if file is a directory
    if (entry.attributes & FAT32_ATTR_DIRECTORY) {
        return ERR_IS_DIRECTORY;
    }
    
    // Allocate file handle
    fat32_file_t* file = fat32_alloc_file();
    if (!file) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Initialize file handle
    file->fs = fs;
    memcpy(&file->entry, &entry, sizeof(fat32_dir_entry_t));
    file->size = entry.file_size;
    file->position = 0;
    file->current_cluster = 0;
    file->cluster_buffer = NULL;
    file->cluster_dirty = false;
    
    // Load first cluster
    uint32_t first_cluster = (entry.cluster_low) | ((uint32_t)entry.cluster_high << 16);
    if (first_cluster >= 2 && file->size > 0) {
        err = fat32_load_cluster(file, first_cluster);
        if (err != ERR_OK) {
            fat32_free_file(file);
            return err;
        }
    }
    
    // Return file handle as FD (simplified - in real system, would use FD table)
    *fd = (fd_t)(uintptr_t)file;
    
    return ERR_OK;
}

/**
 * FAT32 close file
 */
error_code_t fat32_file_close(fat32_fs_t* fs, fd_t fd) {
    (void)fs;  // Unused for now
    
    fat32_file_t* file = (fat32_file_t*)(uintptr_t)fd;
    if (!file) {
        return ERR_INVALID_ARG;
    }
    
    fat32_free_file(file);
    return ERR_OK;
}

/**
 * FAT32 read file
 */
error_code_t fat32_file_read(fat32_fs_t* fs, fd_t fd, void* buf, size_t count, size_t* bytes_read) {
    (void)fs;  // Unused for now
    
    fat32_file_t* file = (fat32_file_t*)(uintptr_t)fd;
    if (!file || !buf || !bytes_read) {
        return ERR_INVALID_ARG;
    }
    
    // Check bounds
    if (file->position >= file->size) {
        *bytes_read = 0;
        return ERR_OK;
    }
    
    // Calculate how much we can read
    size_t remaining = file->size - file->position;
    size_t to_read = (count < remaining) ? count : remaining;
    size_t total_read = 0;
    
    uint8_t* dest = (uint8_t*)buf;
    
    // Read data cluster by cluster
    while (to_read > 0) {
        // Get current cluster
        uint32_t cluster = fat32_get_cluster_for_position(file, file->position);
        if (cluster < 2) {
            break;
        }
        
        // Load cluster if needed
        if (file->current_cluster != cluster) {
            error_code_t err = fat32_load_cluster(file, cluster);
            if (err != ERR_OK) {
                break;
            }
        }
        
        // Calculate offset within cluster
        uint32_t cluster_offset = file->position % file->fs->bytes_per_cluster;
        size_t available_in_cluster = file->fs->bytes_per_cluster - cluster_offset;
        size_t read_from_cluster = (to_read < available_in_cluster) ? to_read : available_in_cluster;
        
        // Copy data
        memcpy(dest, file->cluster_buffer + cluster_offset, read_from_cluster);
        
        dest += read_from_cluster;
        file->position += read_from_cluster;
        total_read += read_from_cluster;
        to_read -= read_from_cluster;
    }
    
    *bytes_read = total_read;
    return ERR_OK;
}

/**
 * FAT32 write file
 */
error_code_t fat32_file_write(fat32_fs_t* fs, fd_t fd, const void* buf, size_t count, size_t* bytes_written) {
    (void)fs;  // Unused for now
    
    fat32_file_t* file = (fat32_file_t*)(uintptr_t)fd;
    if (!file || !buf || !bytes_written) {
        return ERR_INVALID_ARG;
    }
    
    // Check write permission (if file is read-only, deny write)
    if (file->entry.attributes & FAT32_ATTR_READ_ONLY) {
        return ERR_PERMISSION_DENIED;
    }
    
    size_t total_written = 0;
    const uint8_t* src = (const uint8_t*)buf;
    
    // Write data cluster by cluster
    while (count > 0) {
        // Get or allocate cluster for current position
        uint32_t cluster = fat32_get_cluster_for_position(file, file->position);
        
        // If position is beyond file, allocate new clusters
        if (cluster < 2) {
            // Allocate new cluster
            cluster = fat32_alloc_cluster(file->fs);
            if (cluster < 2) {
                break;  // Out of space
            }
            
            // Link to previous cluster if exists
            // TODO: Link new cluster to chain
            
            // Initialize cluster
            if (!file->cluster_buffer) {
                file->cluster_buffer = (uint8_t*)kmalloc(file->fs->bytes_per_cluster);
                if (!file->cluster_buffer) {
                    break;
                }
            }
            memset(file->cluster_buffer, 0, file->fs->bytes_per_cluster);
        }
        
        // Load cluster if needed
        if (file->current_cluster != cluster) {
            error_code_t err = fat32_load_cluster(file, cluster);
            if (err != ERR_OK) {
                break;
            }
        }
        
        // Calculate offset within cluster
        uint32_t cluster_offset = file->position % file->fs->bytes_per_cluster;
        size_t available_in_cluster = file->fs->bytes_per_cluster - cluster_offset;
        size_t write_to_cluster = (count < available_in_cluster) ? count : available_in_cluster;
        
        // Copy data
        memcpy(file->cluster_buffer + cluster_offset, src, write_to_cluster);
        file->cluster_dirty = true;
        
        src += write_to_cluster;
        file->position += write_to_cluster;
        total_written += write_to_cluster;
        count -= write_to_cluster;
        
        // Update file size if we extended it
        if (file->position > file->size) {
            file->size = file->position;
        }
    }
    
    *bytes_written = total_written;
    return ERR_OK;
}

/**
 * FAT32 seek file
 */
error_code_t fat32_file_seek(fat32_fs_t* fs, fd_t fd, int64_t offset, int whence) {
    (void)fs;  // Unused for now
    
    fat32_file_t* file = (fat32_file_t*)(uintptr_t)fd;
    if (!file) {
        return ERR_INVALID_ARG;
    }
    
    uint64_t new_position;
    
    switch (whence) {
        case 0:  // SEEK_SET
            new_position = (uint64_t)offset;
            break;
        case 1:  // SEEK_CUR
            new_position = file->position + offset;
            break;
        case 2:  // SEEK_END
            new_position = file->size + offset;
            break;
        default:
            return ERR_INVALID_ARG;
    }
    
    // Clamp to file size
    if (new_position > file->size) {
        new_position = file->size;
    }
    
    file->position = new_position;
    return ERR_OK;
}

/**
 * FAT32 tell file position
 */
error_code_t fat32_file_tell(fat32_fs_t* fs, fd_t fd, size_t* position) {
    (void)fs;  // Unused for now
    
    fat32_file_t* file = (fat32_file_t*)(uintptr_t)fd;
    if (!file || !position) {
        return ERR_INVALID_ARG;
    }
    
    *position = file->position;
    return ERR_OK;
}

