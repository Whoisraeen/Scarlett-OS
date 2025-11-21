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
static error_code_t ntfs_vfs_open(vfs_filesystem_t* fs, const char* path, uint64_t flags, fd_t* fd) {
    if (!fs || !fs->private_data || !path || !fd) {
        return ERR_INVALID_ARG;
    }
    
    // Check for write flags
    if (flags & VFS_MODE_WRITE) {
        return ERR_READ_ONLY;
    }
    
    ntfs_fs_t* ntfs_fs = (ntfs_fs_t*)fs->private_data;
    
    // TODO: Resolve path to MFT record
    (void)ntfs_fs;
    (void)flags;
    
    return ERR_NOT_IMPLEMENTED;
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
    
    // TODO: Get MFT record from fd and read data
    (void)ntfs_fs;
    (void)fd;
    (void)count;
    
    *bytes_read = 0;
    return ERR_NOT_IMPLEMENTED;
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
    return ERR_NOT_IMPLEMENTED;
}

/**
 * NTFS tell operation
 */
static error_code_t ntfs_vfs_tell(vfs_filesystem_t* fs, fd_t fd, size_t* position) {
    (void)fs;
    (void)fd;
    (void)position;
    return ERR_NOT_IMPLEMENTED;
}

/**
 * NTFS stat operation
 */
static error_code_t ntfs_vfs_stat(vfs_filesystem_t* fs, const char* path, vfs_stat_t* stat) {
    if (!fs || !fs->private_data || !path || !stat) {
        return ERR_INVALID_ARG;
    }
    
    ntfs_fs_t* ntfs_fs = (ntfs_fs_t*)fs->private_data;
    
    // TODO: Resolve path and read MFT record
    (void)ntfs_fs;
    
    return ERR_NOT_IMPLEMENTED;
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

