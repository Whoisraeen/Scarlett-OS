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
#include "../include/fs/permissions.h"
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
 * FAT32 stat implementation
 */
static error_code_t fat32_stat(vfs_filesystem_t* fs, const char* path, vfs_stat_t* stat) {
    if (!fs || !fs->private_data || !path || !stat) {
        return ERR_INVALID_ARG;
    }
    
    fat32_fs_t* fat32_fs = (fat32_fs_t*)fs->private_data;
    
    // Find file
    fat32_dir_entry_t entry;
    error_code_t err = fat32_find_file(fat32_fs, path, &entry);
    if (err != ERR_OK) {
        return err;
    }
    
    // Fill stat structure
    stat->ino = (entry.cluster_low | ((uint64_t)entry.cluster_high << 16));
    stat->type = (entry.attributes & FAT32_ATTR_DIRECTORY) ? VFS_TYPE_DIRECTORY : VFS_TYPE_FILE;
    stat->size = entry.file_size;
    
    // Convert FAT32 attributes to Unix permissions
    // FAT32 doesn't have full permission model, so use defaults
    if (entry.attributes & FAT32_ATTR_READ_ONLY) {
        stat->mode = PERM_OWNER_READ | PERM_GROUP_READ | PERM_OTHER_READ;  // 0444
    } else if (entry.attributes & FAT32_ATTR_DIRECTORY) {
        stat->mode = PERM_DEFAULT_DIR;  // 0755
    } else {
        stat->mode = PERM_DEFAULT_FILE;  // 0644
    }
    
    stat->uid = 0;  // Root for now
    stat->gid = 0;  // Root group for now
    stat->atime = 0;  // TODO: Convert FAT32 dates
    stat->mtime = 0;  // TODO: Convert FAT32 dates
    stat->ctime = 0;  // TODO: Convert FAT32 dates
    
    return ERR_OK;
}

/**
 * FAT32 mkdir VFS wrapper
 */
static error_code_t fat32_vfs_mkdir(vfs_filesystem_t* fs, const char* path) {
    if (!fs || !fs->private_data || !path) {
        return ERR_INVALID_ARG;
    }
    fat32_fs_t* fat32_fs = (fat32_fs_t*)fs->private_data;
    return fat32_mkdir(fat32_fs, path);
}

/**
 * FAT32 rmdir VFS wrapper
 */
static error_code_t fat32_vfs_rmdir(vfs_filesystem_t* fs, const char* path) {
    if (!fs || !fs->private_data || !path) {
        return ERR_INVALID_ARG;
    }
    fat32_fs_t* fat32_fs = (fat32_fs_t*)fs->private_data;
    return fat32_rmdir(fat32_fs, path);
}

/**
 * FAT32 opendir VFS wrapper
 */
static error_code_t fat32_vfs_opendir(vfs_filesystem_t* fs, const char* path, fd_t* fd) {
    if (!fs || !fs->private_data || !path || !fd) {
        return ERR_INVALID_ARG;
    }
    fat32_fs_t* fat32_fs = (fat32_fs_t*)fs->private_data;
    return fat32_opendir(fat32_fs, path, fd);
}

/**
 * FAT32 readdir VFS wrapper
 */
static error_code_t fat32_vfs_readdir(vfs_filesystem_t* fs, fd_t fd, vfs_dirent_t* entry) {
    if (!fs || !fs->private_data || !entry) {
        return ERR_INVALID_ARG;
    }
    fat32_fs_t* fat32_fs = (fat32_fs_t*)fs->private_data;
    return fat32_readdir(fat32_fs, fd, entry);
}

/**
 * FAT32 closedir VFS wrapper
 */
static error_code_t fat32_vfs_closedir(vfs_filesystem_t* fs, fd_t fd) {
    if (!fs || !fs->private_data) {
        return ERR_INVALID_ARG;
    }
    fat32_fs_t* fat32_fs = (fat32_fs_t*)fs->private_data;
    return fat32_closedir(fat32_fs, fd);
}

/**
 * FAT32 unlink VFS wrapper
 */
static error_code_t fat32_vfs_unlink(vfs_filesystem_t* fs, const char* path) {
    if (!fs || !fs->private_data || !path) {
        return ERR_INVALID_ARG;
    }
    fat32_fs_t* fat32_fs = (fat32_fs_t*)fs->private_data;
    return fat32_delete_file(fat32_fs, path);
}

/**
 * FAT32 rename VFS wrapper
 */
static error_code_t fat32_vfs_rename(vfs_filesystem_t* fs, const char* oldpath, const char* newpath) {
    if (!fs || !fs->private_data || !oldpath || !newpath) {
        return ERR_INVALID_ARG;
    }
    // TODO: Implement rename
    (void)fs;
    (void)oldpath;
    (void)newpath;
    return ERR_NOT_SUPPORTED;
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
    .mkdir = fat32_vfs_mkdir,
    .rmdir = fat32_vfs_rmdir,
    .opendir = fat32_vfs_opendir,
    .readdir = fat32_vfs_readdir,
    .closedir = fat32_vfs_closedir,
    .unlink = fat32_vfs_unlink,
    .rename = fat32_vfs_rename,
    .stat = fat32_stat,
    .private_data = NULL
};

/**
 * Register FAT32 filesystem with VFS
 */
error_code_t fat32_register_vfs(void) {
    kinfo("Registering FAT32 filesystem with VFS...\n");
    return vfs_register_filesystem(&fat32_vfs_filesystem);
}

