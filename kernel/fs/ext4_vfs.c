/**
 * @file ext4_vfs.c
 * @brief ext4 VFS integration
 */

#include "../include/fs/ext4.h"
#include "../include/fs/vfs.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"

// Global ext4 filesystem instance
static ext4_fs_t* g_ext4_fs = NULL;

/**
 * ext4 mount operation
 */
static error_code_t ext4_vfs_mount(vfs_filesystem_t* fs, const char* device, const char* mountpoint) {
    kinfo("ext4: Mounting device %s at %s\n", device, mountpoint);
    
    // Find block device
    block_device_t* block_dev = block_device_get(device);
    if (!block_dev) {
        kerror("ext4: Device %s not found\n", device);
        return ERR_DEVICE_NOT_FOUND;
    }
    
    // Allocate ext4 filesystem
    ext4_fs_t* ext4_fs = (ext4_fs_t*)kmalloc(sizeof(ext4_fs_t));
    if (!ext4_fs) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Initialize ext4
    error_code_t err = ext4_init(block_dev, ext4_fs);
    if (err != ERR_OK) {
        kfree(ext4_fs);
        return err;
    }
    
    // Mount
    err = ext4_mount(ext4_fs, mountpoint);
    if (err != ERR_OK) {
        kfree(ext4_fs);
        return err;
    }
    
    fs->private_data = ext4_fs;
    g_ext4_fs = ext4_fs;
    
    kinfo("ext4: Mounted successfully\n");
    return ERR_OK;
}

/**
 * ext4 unmount operation
 */
static error_code_t ext4_vfs_unmount(vfs_filesystem_t* fs) {
    if (!fs || !fs->private_data) {
        return ERR_INVALID_ARG;
    }
    
    ext4_fs_t* ext4_fs = (ext4_fs_t*)fs->private_data;
    ext4_unmount(ext4_fs);
    kfree(ext4_fs);
    g_ext4_fs = NULL;
    
    return ERR_OK;
}

/**
 * ext4 open operation
 */
static error_code_t ext4_vfs_open(vfs_filesystem_t* fs, const char* path, uint64_t flags, fd_t* fd) {
    if (!fs || !fs->private_data || !path || !fd) {
        return ERR_INVALID_ARG;
    }
    
    ext4_fs_t* ext4_fs = (ext4_fs_t*)fs->private_data;
    
    // TODO: Resolve path to inode number
    // For now, return error
    (void)ext4_fs;
    (void)flags;
    
    return ERR_NOT_SUPPORTED;
}

/**
 * ext4 close operation
 */
static error_code_t ext4_vfs_close(vfs_filesystem_t* fs, fd_t fd) {
    (void)fs;
    (void)fd;
    return ERR_OK;
}

static error_code_t ext4_vfs_read(vfs_filesystem_t* fs, fd_t fd, void* buf, size_t count, size_t* bytes_read) {
    if (!fs || !fs->private_data || !buf || !bytes_read) {
        return ERR_INVALID_ARG;
    }
    
    // Get inode from fd and read data
    extern fd_entry_t fd_table[];
    
    if (fd < 0 || fd >= 256 || !fd_table[fd].used) {
        return ERR_INVALID_ARG;
    }
    
    uint32_t inode_num = (uint32_t)(uintptr_t)fd_table[fd].file_data;
    if (inode_num == 0) {
        return ERR_NOT_FOUND;
    }
    
    ext4_fs_t* ext4_fs = (ext4_fs_t*)fs->private_data;
    
    // Use current position from fd
    size_t offset = fd_table[fd].position;
    error_code_t err = ext4_read_file(ext4_fs, inode_num, buf, offset, count, bytes_read);
    
    if (err == ERR_OK) {
        // Update position
        fd_table[fd].position += *bytes_read;
    }
    
    return err;
}

static error_code_t ext4_vfs_write(vfs_filesystem_t* fs, fd_t fd, const void* buf, size_t count, size_t* bytes_written) {
    (void)fs;
    (void)fd;
    (void)buf;
    (void)count;
    (void)bytes_written;
    return ERR_NOT_SUPPORTED;  // Read-only for now
}

/**
 * ext4 seek operation
 */
static error_code_t ext4_vfs_seek(vfs_filesystem_t* fs, fd_t fd, int64_t offset, int whence) {
    (void)fs;
    (void)fd;
    (void)offset;
    (void)whence;
    return ERR_NOT_SUPPORTED;
}

/**
 * ext4 tell operation
 */
static error_code_t ext4_vfs_tell(vfs_filesystem_t* fs, fd_t fd, size_t* position) {
    (void)fs;
    (void)fd;
    (void)position;
    return ERR_NOT_SUPPORTED;
}

/**
 * ext4 stat operation
 */
static error_code_t ext4_vfs_stat(vfs_filesystem_t* fs, const char* path, vfs_stat_t* stat) {
    if (!fs || !fs->private_data || !path || !stat) {
        return ERR_INVALID_ARG;
    }
    
    ext4_fs_t* ext4_fs = (ext4_fs_t*)fs->private_data;
    
    // Resolve path and read inode
    // Parse path components (same as in ext4_vfs_open)
    char components[64][256];
    uint32_t component_count = 0;
    
    // Skip leading slash
    const char* p = path;
    if (*p == '/') {
        p++;
    }
    
    // Parse path into components
    const char* start = p;
    while (*p) {
        if (*p == '/') {
            if (p > start && component_count < 64) {
                size_t len = p - start;
                if (len < 256) {
                    memcpy(components[component_count], start, len);
                    components[component_count][len] = '\0';
                    component_count++;
                }
            }
            start = p + 1;
        }
        p++;
    }
    
    // Add final component
    if (p > start && component_count < 64) {
        size_t len = p - start;
        if (len < 256) {
            memcpy(components[component_count], start, len);
            components[component_count][len] = '\0';
            component_count++;
        }
    }
    
    // Start from root inode (inode 2)
    uint32_t current_inode = 2;
    
    // Traverse path
    for (uint32_t i = 0; i < component_count; i++) {
        uint32_t next_inode;
        error_code_t err = ext4_find_file(ext4_fs, current_inode, components[i], &next_inode);
        if (err != ERR_OK) {
            return err;
        }
        current_inode = next_inode;
    }
    
    // Read inode
    ext4_inode_t inode;
    error_code_t err = ext4_read_inode(ext4_fs, current_inode, &inode);
    if (err != ERR_OK) {
        return err;
    }
    
    // Fill stat structure
    stat->ino = current_inode;
    stat->type = ((inode.mode & 0xF000) == 0x4000) ? VFS_TYPE_DIRECTORY : VFS_TYPE_FILE;
    stat->size = inode.size_lo | ((uint64_t)inode.size_hi << 32);
    stat->mode = inode.mode & 0x0FFF;
    stat->uid = inode.uid | ((uint64_t)inode.uid_hi << 16);
    stat->gid = inode.gid | ((uint64_t)inode.gid_hi << 16);
    stat->atime = inode.atime;
    stat->mtime = inode.mtime;
    stat->ctime = inode.ctime;
    
    return ERR_OK;
}

/**
 * Register ext4 filesystem with VFS
 */
error_code_t ext4_register_vfs(void) {
    static vfs_filesystem_t ext4_fs = {0};
    
    ext4_fs.name = "ext4";
    ext4_fs.mount = ext4_vfs_mount;
    ext4_fs.unmount = ext4_vfs_unmount;
    ext4_fs.open = ext4_vfs_open;
    ext4_fs.close = ext4_vfs_close;
    ext4_fs.read = ext4_vfs_read;
    ext4_fs.write = ext4_vfs_write;
    ext4_fs.seek = ext4_vfs_seek;
    ext4_fs.tell = ext4_vfs_tell;
    ext4_fs.stat = ext4_vfs_stat;
    ext4_fs.private_data = NULL;
    
    extern error_code_t vfs_register_filesystem(vfs_filesystem_t* fs);
    return vfs_register_filesystem(&ext4_fs);
}

