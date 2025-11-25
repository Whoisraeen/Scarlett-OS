/**
 * @file ntfs_vfs.c
 * @brief NTFS VFS integration (read-only)
 */

#include "../include/fs/ntfs.h"
#include "../include/fs/vfs.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"

// Global NTFS filesystem instance
static ntfs_fs_t* g_ntfs_fs = NULL;

/**
 * NTFS mount operation
 */
static error_code_t ntfs_vfs_mount(vfs_filesystem_t* fs, const char* device, const char* mountpoint) {
    kinfo("NTFS: Mounting device %s at %s (read-only)\n", device, mountpoint);
    
    // Find block device
    block_device_t* block_dev = block_device_get(device);
    if (!block_dev) {
        kerror("NTFS: Device %s not found\n", device);
        return ERR_DEVICE_NOT_FOUND;
    }
    
    // Allocate NTFS filesystem
    ntfs_fs_t* ntfs_fs = (ntfs_fs_t*)kmalloc(sizeof(ntfs_fs_t));
    if (!ntfs_fs) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Initialize NTFS
    error_code_t err = ntfs_init(block_dev, ntfs_fs);
    if (err != ERR_OK) {
        kfree(ntfs_fs);
        return err;
    }
    
    // Mount
    err = ntfs_mount(ntfs_fs, mountpoint);
    if (err != ERR_OK) {
        kfree(ntfs_fs);
        return err;
    }
    
    fs->private_data = ntfs_fs;
    g_ntfs_fs = ntfs_fs;
    
    kinfo("NTFS: Mounted successfully (read-only)\n");
    return ERR_OK;
}

/**
 * NTFS unmount operation
 */
static error_code_t ntfs_vfs_unmount(vfs_filesystem_t* fs) {
    if (!fs || !fs->private_data) {
        return ERR_INVALID_ARG;
    }
    
    ntfs_fs_t* ntfs_fs = (ntfs_fs_t*)fs->private_data;
    ntfs_unmount(ntfs_fs);
    kfree(ntfs_fs);
    g_ntfs_fs = NULL;
    
    return ERR_OK;
}

/**
 * NTFS open operation (read-only)
 */
static error_code_t ntfs_vfs_open(vfs_filesystem_t* fs, const char* path, uint64_t flags, fd_t* fd, void** file_data) {
    if (!fs || !fs->private_data || !path || !fd || !file_data) {
        return ERR_INVALID_ARG;
    }
    
    // Check for write flags
    if (flags & VFS_MODE_WRITE) {
        return ERR_READ_ONLY;
    }
    
    ntfs_fs_t* ntfs_fs = (ntfs_fs_t*)fs->private_data;
    
    // Resolve path to MFT record
    uint64_t mft_record;
    error_code_t err = ntfs_find_file(ntfs_fs, path, &mft_record);
    if (err != ERR_OK) {
        return err;
    }
    
    // Find free fd slot (VFS handles this now, but we need to return the fd index if we were allocating it)
    // Wait, vfs_open allocates the FD and passes it to us.
    // So we don't need to allocate it.
    // We just need to set file_data.
    
    *file_data = (void*)(uintptr_t)mft_record;
    
    // Note: fd is already set by vfs_open before calling us?
    // vfs_open: `fd_t new_fd = allocate_fd();` then `mount->fs->open(..., &new_fd, ...)`
    // So *fd contains the new_fd. We don't need to change it.
    
    return ERR_OK;
}

/**
 * NTFS close operation
 */
static error_code_t ntfs_vfs_close(vfs_filesystem_t* fs, fd_t fd) {
    (void)fs;
    (void)fd;
    return ERR_OK;
}

/**
 * NTFS read operation
 */
static error_code_t ntfs_vfs_read(vfs_filesystem_t* fs, fd_t fd, void* buf, size_t count, size_t* bytes_read) {
    if (!fs || !fs->private_data || !buf || !bytes_read) {
        return ERR_INVALID_ARG;
    }
    
    ntfs_fs_t* ntfs_fs = (ntfs_fs_t*)fs->private_data;
    
    // Get MFT record from fd and read data
    uint64_t mft_record = (uint64_t)(uintptr_t)vfs_get_file_data(fd);
    if (mft_record == 0) {
        return ERR_NOT_FOUND;
    }
    
    // Use current position from fd
    size_t offset = (size_t)vfs_get_position(fd);
    error_code_t err = ntfs_read_file(ntfs_fs, mft_record, buf, offset, count, bytes_read);
    
    // Position update is handled by VFS
    
    return err;
}

/**
 * NTFS write operation (not supported - read-only)
 */
static error_code_t ntfs_vfs_write(vfs_filesystem_t* fs, fd_t fd, const void* buf, size_t count, size_t* bytes_written) {
    (void)fs;
    (void)fd;
    (void)buf;
    (void)count;
    (void)bytes_written;
    return ERR_READ_ONLY;
}

/**
 * NTFS seek operation
 */
static error_code_t ntfs_vfs_seek(vfs_filesystem_t* fs, fd_t fd, int64_t offset, int whence) {
    (void)fs;
    (void)fd;
    (void)offset;
    (void)whence;
    return ERR_NOT_SUPPORTED;
}

/**
 * NTFS tell operation
 */
static error_code_t ntfs_vfs_tell(vfs_filesystem_t* fs, fd_t fd, size_t* position) {
    (void)fs;
    (void)fd;
    (void)position;
    return ERR_NOT_SUPPORTED;
}

/**
 * NTFS stat operation
 */
static error_code_t ntfs_vfs_stat(vfs_filesystem_t* fs, const char* path, vfs_stat_t* stat) {
    if (!fs || !fs->private_data || !path || !stat) {
        return ERR_INVALID_ARG;
    }
    
    ntfs_fs_t* ntfs_fs = (ntfs_fs_t*)fs->private_data;
    
    // Resolve path and read MFT record
    uint64_t mft_record;
    error_code_t err = ntfs_find_file(ntfs_fs, path, &mft_record);
    if (err != ERR_OK) {
        return err;
    }
    
    // Read MFT record
    uint8_t* mft_buffer = (uint8_t*)kmalloc(ntfs_fs->mft_record_size);
    if (!mft_buffer) {
        return ERR_OUT_OF_MEMORY;
    }
    
    err = ntfs_read_mft_record(ntfs_fs, mft_record, mft_buffer);
    if (err != ERR_OK) {
        kfree(mft_buffer);
        return err;
    }
    
    // Parse attributes to fill stat structure
    ntfs_mft_record_t* record = (ntfs_mft_record_t*)mft_buffer;
    
    // Find $STANDARD_INFORMATION attribute (type 0x10) for timestamps
    uint16_t attr_offset = record->attribute_offset;
    uint8_t* attr_ptr = mft_buffer + attr_offset;
    uint64_t file_size = 0;
    uint64_t mtime = 0;
    uint64_t atime = 0;
    uint64_t ctime = 0;
    
    while (attr_offset < ntfs_fs->mft_record_size) {
        uint32_t attr_type = *(uint32_t*)attr_ptr;
        uint32_t attr_length = *(uint32_t*)(attr_ptr + 4);
        
        if (attr_type == 0xFFFFFFFF) {
            break;
        }
        
        if (attr_type == NTFS_ATTR_STANDARD_INFORMATION) {
            // Extract timestamps (simplified - actual structure is more complex)
            // Standard information contains timestamps at specific offsets
            // This is a simplified extraction
            if (attr_ptr[8] == 0) {  // Resident
                // Timestamps are at offsets 24, 32, 40, 48 (mtime, atime, ctime, mft_change_time)
                // NTFS uses Windows FILETIME format (100-nanosecond intervals since 1601-01-01)
                // For now, extract as-is
                mtime = *(uint64_t*)(attr_ptr + 24);
                atime = *(uint64_t*)(attr_ptr + 32);
                ctime = *(uint64_t*)(attr_ptr + 40);
            }
        } else if (attr_type == NTFS_ATTR_DATA) {
            // Extract file size
            if (attr_ptr[8] == 0) {  // Resident
                file_size = *(uint32_t*)(attr_ptr + 16);
            } else {  // Non-resident
                file_size = *(uint64_t*)(attr_ptr + 48);
            }
        }
        
        attr_offset += attr_length;
        attr_ptr += attr_length;
    }
    
    // Fill stat structure
    stat->ino = mft_record;
    stat->type = VFS_TYPE_FILE;  // Would need to check from $FILE_NAME attribute
    stat->size = file_size;
    stat->mode = 0644;  // Default permissions
    stat->uid = 0;
    stat->gid = 0;
    stat->atime = atime / 10000000 - 11644473600ULL;  // Convert FILETIME to Unix timestamp (simplified)
    stat->mtime = mtime / 10000000 - 11644473600ULL;
    stat->ctime = ctime / 10000000 - 11644473600ULL;
    
    kfree(mft_buffer);
    return ERR_OK;
}

/**
 * Register NTFS filesystem with VFS
 */
error_code_t ntfs_register_vfs(void) {
    static vfs_filesystem_t ntfs_fs = {0};
    
    ntfs_fs.name = "ntfs";
    ntfs_fs.mount = ntfs_vfs_mount;
    ntfs_fs.unmount = ntfs_vfs_unmount;
    ntfs_fs.open = ntfs_vfs_open;
    ntfs_fs.close = ntfs_vfs_close;
    ntfs_fs.read = ntfs_vfs_read;
    ntfs_fs.write = ntfs_vfs_write;
    ntfs_fs.seek = ntfs_vfs_seek;
    ntfs_fs.tell = ntfs_vfs_tell;
    ntfs_fs.stat = ntfs_vfs_stat;
    ntfs_fs.private_data = NULL;
    
    extern error_code_t vfs_register_filesystem(vfs_filesystem_t* fs);
    return vfs_register_filesystem(&ntfs_fs);
}

