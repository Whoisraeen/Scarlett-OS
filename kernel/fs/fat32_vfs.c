/**
 * @file fat32_vfs.c
 * @brief FAT32 VFS integration
 * 
 * Connects FAT32 filesystem to VFS layer.
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

// Global FAT32 filesystem instance (simplified - would support multiple mounts)
static fat32_fs_t* g_fat32_fs = NULL;

/**
 * FAT32 mount operation
 */
static error_code_t fat32_vfs_mount(vfs_filesystem_t* fs, const char* device, const char* mountpoint) {
    (void)mountpoint;  // For now
    
    kinfo("FAT32: Mounting device %s at %s\n", device, mountpoint);
    
    // Get block device
    block_device_t* block_dev = block_device_get(device);
    if (!block_dev) {
        kerror("FAT32: Device %s not found\n", device);
        return ERR_NOT_FOUND;
    }
    
    // Allocate filesystem structure
    fat32_fs_t* fat32_fs = (fat32_fs_t*)kmalloc(sizeof(fat32_fs_t));
    if (!fat32_fs) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Initialize FAT32
    error_code_t err = fat32_init(block_dev, fat32_fs);
    if (err != ERR_OK) {
        kfree(fat32_fs);
        return err;
    }
    
    // Store in filesystem private data
    fs->private_data = fat32_fs;
    g_fat32_fs = fat32_fs;
    
    kinfo("FAT32: Mounted successfully\n");
    return ERR_OK;
}

/**
 * FAT32 unmount operation
 */
static error_code_t fat32_vfs_unmount(vfs_filesystem_t* fs) {
    if (!fs || !fs->private_data) {
        return ERR_INVALID_ARG;
    }
    
    fat32_fs_t* fat32_fs = (fat32_fs_t*)fs->private_data;
    
    // Free FAT cache
    if (fat32_fs->fat_cache) {
        kfree(fat32_fs->fat_cache);
    }
    
    // Free filesystem structure
    kfree(fat32_fs);
    fs->private_data = NULL;
    g_fat32_fs = NULL;
    
    return ERR_OK;
}

/**
 * FAT32 open operation
 */
static error_code_t fat32_vfs_open(vfs_filesystem_t* fs, const char* path, uint64_t flags, fd_t* fd) {
    if (!fs || !fs->private_data || !path || !fd) {
        return ERR_INVALID_ARG;
    }
    
    fat32_fs_t* fat32_fs = (fat32_fs_t*)fs->private_data;
    return fat32_file_open(fat32_fs, path, flags, fd);
}

/**
 * FAT32 close operation
 */
static error_code_t fat32_vfs_close(vfs_filesystem_t* fs, fd_t fd) {
    if (!fs || !fs->private_data) {
        return ERR_INVALID_ARG;
    }
    
    fat32_fs_t* fat32_fs = (fat32_fs_t*)fs->private_data;
    return fat32_file_close(fat32_fs, fd);
}

/**
 * FAT32 read operation
 */
static error_code_t fat32_vfs_read(vfs_filesystem_t* fs, fd_t fd, void* buf, size_t count, size_t* bytes_read) {
    if (!fs || !fs->private_data || !buf || !bytes_read) {
        return ERR_INVALID_ARG;
    }
    
    fat32_fs_t* fat32_fs = (fat32_fs_t*)fs->private_data;
    return fat32_file_read(fat32_fs, fd, buf, count, bytes_read);
}

/**
 * FAT32 write operation
 */
static error_code_t fat32_vfs_write(vfs_filesystem_t* fs, fd_t fd, const void* buf, size_t count, size_t* bytes_written) {
    if (!fs || !fs->private_data || !buf || !bytes_written) {
        return ERR_INVALID_ARG;
    }
    
    fat32_fs_t* fat32_fs = (fat32_fs_t*)fs->private_data;
    return fat32_file_write(fat32_fs, fd, buf, count, bytes_written);
}

/**
 * FAT32 seek operation
 */
static error_code_t fat32_vfs_seek(vfs_filesystem_t* fs, fd_t fd, int64_t offset, int whence) {
    if (!fs || !fs->private_data) {
        return ERR_INVALID_ARG;
    }
    
    fat32_fs_t* fat32_fs = (fat32_fs_t*)fs->private_data;
    return fat32_file_seek(fat32_fs, fd, offset, whence);
}

/**
 * FAT32 tell operation
 */
static error_code_t fat32_vfs_tell(vfs_filesystem_t* fs, fd_t fd, size_t* position) {
    if (!fs || !fs->private_data || !position) {
        return ERR_INVALID_ARG;
    }
    
    fat32_fs_t* fat32_fs = (fat32_fs_t*)fs->private_data;
    return fat32_file_tell(fat32_fs, fd, position);
}

/**
 * FAT32 VFS filesystem structure
 */
vfs_filesystem_t fat32_vfs_filesystem = {
    .name = "fat32",
    .mount = fat32_vfs_mount,
    .unmount = fat32_vfs_unmount,
    .open = fat32_vfs_open,
    .close = fat32_vfs_close,
    .read = fat32_vfs_read,
    .write = fat32_vfs_write,
    .seek = fat32_vfs_seek,
    .tell = fat32_vfs_tell,
    .mkdir = NULL,  // TODO
    .rmdir = NULL,  // TODO
    .opendir = NULL,  // TODO
    .readdir = NULL,  // TODO
    .closedir = NULL,  // TODO
    .unlink = NULL,  // TODO
    .rename = NULL,  // TODO
    .stat = fat32_stat,  // Implement stat
    .private_data = NULL
};

/**
 * Register FAT32 filesystem with VFS
 */
error_code_t fat32_register_vfs(void) {
    kinfo("Registering FAT32 filesystem with VFS...\n");
    return vfs_register_filesystem(&fat32_vfs_filesystem);
}

