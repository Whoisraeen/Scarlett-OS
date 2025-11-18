/**
 * @file vfs.c
 * @brief Virtual File System implementation
 */

#include "../include/types.h"
#include "../include/fs/vfs.h"
#include "../include/fs/permissions.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

// Maximum file descriptors
#define MAX_FDS 256

// File descriptor entry
typedef struct {
    bool used;                  // Is this FD in use?
    vfs_filesystem_t* fs;       // Filesystem
    void* file_data;            // Filesystem-specific file data
    uint64_t position;          // Current file position
    uint64_t flags;             // Open flags
} fd_entry_t;

// File descriptor table
static fd_entry_t fd_table[MAX_FDS];

// Registered filesystems
static vfs_filesystem_t* filesystems = NULL;

// Mount points
static vfs_mount_t* mount_points = NULL;

// Root mount point
static vfs_mount_t* root_mount = NULL;

/**
 * Initialize VFS
 */
error_code_t vfs_init(void) {
    kinfo("Initializing VFS...\n");
    
    // Clear FD table
    for (int i = 0; i < MAX_FDS; i++) {
        fd_table[i].used = false;
        fd_table[i].fs = NULL;
        fd_table[i].file_data = NULL;
        fd_table[i].position = 0;
        fd_table[i].flags = 0;
    }
    
    filesystems = NULL;
    mount_points = NULL;
    root_mount = NULL;
    
    kinfo("VFS initialized\n");
    return ERR_OK;
}

/**
 * Register a filesystem type
 */
error_code_t vfs_register_filesystem(vfs_filesystem_t* fs) {
    if (!fs || !fs->name) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("Registering filesystem: %s\n", fs->name);
    
    // Add to list
    fs->private_data = NULL;
    // Note: We'd link this into a list, but for now just store it
    filesystems = fs;  // Simplified - would use linked list
    
    return ERR_OK;
}

/**
 * Find filesystem by name
 */
static vfs_filesystem_t* find_filesystem(const char* name) {
    // Simplified - would search linked list
    if (filesystems && filesystems->name) {
        const char* a = filesystems->name;
        const char* b = name;
        bool match = true;
        while (*a && *b) {
            if (*a != *b) {
                match = false;
                break;
            }
            a++;
            b++;
        }
        if (match && *a == '\0' && *b == '\0') {
            return filesystems;
        }
    }
    return NULL;
}

/**
 * Mount a filesystem
 */
error_code_t vfs_mount(const char* device, const char* mountpoint, const char* fstype) {
    if (!device || !mountpoint || !fstype) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("Mounting %s filesystem from %s at %s\n", fstype, device, mountpoint);
    
    // Find filesystem type
    vfs_filesystem_t* fs = find_filesystem(fstype);
    if (!fs) {
        kerror("VFS: Filesystem type '%s' not registered\n", fstype);
        return ERR_NOT_FOUND;
    }
    
    // Create mount point
    // TODO: Allocate mount point structure
    // For now, simplified
    
    // Call filesystem mount function
    if (fs->mount) {
        error_code_t err = fs->mount(fs, device, mountpoint);
        if (err != ERR_OK) {
            return err;
        }
    }
    
    // If mounting at root, set as root mount
    if (mountpoint[0] == '/' && mountpoint[1] == '\0') {
        // TODO: Set root_mount
        kinfo("VFS: Root filesystem mounted\n");
    }
    
    return ERR_OK;
}

/**
 * Resolve path to mount point and relative path
 */
error_code_t vfs_resolve_path(const char* path, vfs_mount_t** mount, char* resolved_path) {
    if (!path || !mount || !resolved_path) {
        return ERR_INVALID_ARG;
    }
    
    // For now, just use root mount
    // TODO: Find appropriate mount point based on path
    *mount = root_mount;
    
    // Copy path (simplified)
    size_t i = 0;
    while (path[i] && i < 255) {
        resolved_path[i] = path[i];
        i++;
    }
    resolved_path[i] = '\0';
    
    return ERR_OK;
}

/**
 * Allocate file descriptor
 */
static fd_t allocate_fd(void) {
    for (int i = 0; i < MAX_FDS; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used = true;
            return i;
        }
    }
    return -1;
}

/**
 * Free file descriptor
 */
static void free_fd(fd_t fd) {
    if (fd >= 0 && fd < MAX_FDS) {
        fd_table[fd].used = false;
        fd_table[fd].fs = NULL;
        fd_table[fd].file_data = NULL;
    }
}

/**
 * Open a file
 */
error_code_t vfs_open(const char* path, uint64_t flags, fd_t* fd) {
    if (!path || !fd) {
        return ERR_INVALID_ARG;
    }
    
    // Resolve path
    vfs_mount_t* mount;
    char resolved_path[256];
    error_code_t err = vfs_resolve_path(path, &mount, resolved_path);
    if (err != ERR_OK || !mount || !mount->fs) {
        return ERR_NOT_FOUND;
    }
    
    // Allocate FD
    fd_t new_fd = allocate_fd();
    if (new_fd < 0) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Get file stat for permission checking
    vfs_stat_t stat;
    if (mount->fs->stat) {
        err = mount->fs->stat(mount->fs, resolved_path, &stat);
        if (err == ERR_OK) {
            // Check permissions based on open flags
            uint32_t uid = permissions_get_current_uid();
            uint32_t gid = permissions_get_current_gid();
            
            file_permissions_t perms;
            perms.mode = (uint16_t)(stat.mode & 0x0FFF);
            perms.uid = stat.uid;
            perms.gid = stat.gid;
            
            // Check read permission
            if (flags & VFS_MODE_READ) {
                if (!permissions_check_read(&perms, uid, gid)) {
                    free_fd(new_fd);
                    return ERR_PERMISSION_DENIED;
                }
            }
            
            // Check write permission
            if (flags & VFS_MODE_WRITE) {
                if (!permissions_check_write(&perms, uid, gid)) {
                    free_fd(new_fd);
                    return ERR_PERMISSION_DENIED;
                }
            }
        }
        // If stat fails, continue anyway (filesystem might not support it yet)
    }
    
    // Call filesystem open
    if (mount->fs->open) {
        err = mount->fs->open(mount->fs, resolved_path, flags, &new_fd);
        if (err != ERR_OK) {
            free_fd(new_fd);
            return err;
        }
    }
    
    // Set up FD entry
    fd_table[new_fd].fs = mount->fs;
    fd_table[new_fd].position = 0;
    fd_table[new_fd].flags = flags;
    
    *fd = new_fd;
    return ERR_OK;
}

/**
 * Close a file
 */
error_code_t vfs_close(fd_t fd) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].used) {
        return ERR_INVALID_ARG;
    }
    
    // Call filesystem close
    if (fd_table[fd].fs && fd_table[fd].fs->close) {
        fd_table[fd].fs->close(fd_table[fd].fs, fd);
    }
    
    free_fd(fd);
    return ERR_OK;
}

/**
 * Read from file
 */
error_code_t vfs_read(fd_t fd, void* buf, size_t count, size_t* bytes_read) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].used || !buf) {
        return ERR_INVALID_ARG;
    }
    
    if (!fd_table[fd].fs || !fd_table[fd].fs->read) {
        return ERR_NOT_SUPPORTED;
    }
    
    // Permission check is done at open time, so we can proceed
    error_code_t err = fd_table[fd].fs->read(fd_table[fd].fs, fd, buf, count, bytes_read);
    if (err == ERR_OK && bytes_read) {
        fd_table[fd].position += *bytes_read;
    }
    
    return err;
}

/**
 * Write to file
 */
error_code_t vfs_write(fd_t fd, const void* buf, size_t count, size_t* bytes_written) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].used || !buf) {
        return ERR_INVALID_ARG;
    }
    
    if (!fd_table[fd].fs || !fd_table[fd].fs->write) {
        return ERR_NOT_SUPPORTED;
    }
    
    // Check write permission (flags checked at open, but verify)
    if (!(fd_table[fd].flags & VFS_MODE_WRITE)) {
        return ERR_PERMISSION_DENIED;
    }
    
    error_code_t err = fd_table[fd].fs->write(fd_table[fd].fs, fd, buf, count, bytes_written);
    if (err == ERR_OK && bytes_written) {
        fd_table[fd].position += *bytes_written;
    }
    
    return err;
}

/**
 * Seek in file
 */
error_code_t vfs_seek(fd_t fd, int64_t offset, int whence) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].used) {
        return ERR_INVALID_ARG;
    }
    
    if (!fd_table[fd].fs || !fd_table[fd].fs->seek) {
        return ERR_NOT_SUPPORTED;
    }
    
    error_code_t err = fd_table[fd].fs->seek(fd_table[fd].fs, fd, offset, whence);
    if (err == ERR_OK && fd_table[fd].fs->tell) {
        fd_table[fd].fs->tell(fd_table[fd].fs, fd, &fd_table[fd].position);
    }
    
    return err;
}

/**
 * Get current file position
 */
error_code_t vfs_tell(fd_t fd, size_t* position) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].used || !position) {
        return ERR_INVALID_ARG;
    }
    
    *position = fd_table[fd].position;
    return ERR_OK;
}

/**
 * Create directory
 */
error_code_t vfs_mkdir(const char* path) {
    if (!path) {
        return ERR_INVALID_ARG;
    }
    
    vfs_mount_t* mount;
    char resolved_path[256];
    error_code_t err = vfs_resolve_path(path, &mount, resolved_path);
    if (err != ERR_OK || !mount || !mount->fs) {
        return err != ERR_OK ? err : ERR_NOT_FOUND;
    }
    
    if (!mount->fs->mkdir) {
        return ERR_NOT_SUPPORTED;
    }
    
    return mount->fs->mkdir(mount->fs, resolved_path);
}

/**
 * Remove directory
 */
error_code_t vfs_rmdir(const char* path) {
    if (!path) {
        return ERR_INVALID_ARG;
    }
    
    vfs_mount_t* mount;
    char resolved_path[256];
    error_code_t err = vfs_resolve_path(path, &mount, resolved_path);
    if (err != ERR_OK || !mount || !mount->fs) {
        return err != ERR_OK ? err : ERR_NOT_FOUND;
    }
    
    if (!mount->fs->rmdir) {
        return ERR_NOT_SUPPORTED;
    }
    
    return mount->fs->rmdir(mount->fs, resolved_path);
}

/**
 * Open directory
 */
error_code_t vfs_opendir(const char* path, fd_t* fd) {
    if (!path || !fd) {
        return ERR_INVALID_ARG;
    }
    
    vfs_mount_t* mount;
    char resolved_path[256];
    error_code_t err = vfs_resolve_path(path, &mount, resolved_path);
    if (err != ERR_OK || !mount || !mount->fs) {
        return err != ERR_OK ? err : ERR_NOT_FOUND;
    }
    
    if (!mount->fs->opendir) {
        return ERR_NOT_SUPPORTED;
    }
    
    return mount->fs->opendir(mount->fs, resolved_path, fd);
}

/**
 * Read directory entry
 */
error_code_t vfs_readdir(fd_t fd, vfs_dirent_t* entry) {
    if (fd < 0 || fd >= MAX_FDS || !entry) {
        return ERR_INVALID_ARG;
    }
    
    // Directory handles are separate from file handles
    // For now, we'll use the filesystem directly
    // TODO: Implement proper directory handle management
    
    // Find filesystem from mount
    if (!root_mount || !root_mount->fs || !root_mount->fs->readdir) {
        return ERR_NOT_SUPPORTED;
    }
    
    return root_mount->fs->readdir(root_mount->fs, fd, entry);
}

/**
 * Close directory
 */
error_code_t vfs_closedir(fd_t fd) {
    if (fd < 0 || fd >= MAX_FDS) {
        return ERR_INVALID_ARG;
    }
    
    if (!root_mount || !root_mount->fs || !root_mount->fs->closedir) {
        return ERR_NOT_SUPPORTED;
    }
    
    return root_mount->fs->closedir(root_mount->fs, fd);
}

/**
 * Delete file
 */
error_code_t vfs_unlink(const char* path) {
    if (!path) {
        return ERR_INVALID_ARG;
    }
    
    vfs_mount_t* mount;
    char resolved_path[256];
    error_code_t err = vfs_resolve_path(path, &mount, resolved_path);
    if (err != ERR_OK || !mount || !mount->fs) {
        return err != ERR_OK ? err : ERR_NOT_FOUND;
    }
    
    if (!mount->fs->unlink) {
        return ERR_NOT_SUPPORTED;
    }
    
    return mount->fs->unlink(mount->fs, resolved_path);
}

/**
 * Rename file
 */
error_code_t vfs_rename(const char* oldpath, const char* newpath) {
    if (!oldpath || !newpath) {
        return ERR_INVALID_ARG;
    }
    
    vfs_mount_t* mount;
    char resolved_old[256], resolved_new[256];
    error_code_t err = vfs_resolve_path(oldpath, &mount, resolved_old);
    if (err != ERR_OK || !mount || !mount->fs) {
        return err != ERR_OK ? err : ERR_NOT_FOUND;
    }
    
    err = vfs_resolve_path(newpath, &mount, resolved_new);
    if (err != ERR_OK) {
        return err;
    }
    
    if (!mount->fs->rename) {
        return ERR_NOT_SUPPORTED;
    }
    
    return mount->fs->rename(mount->fs, resolved_old, resolved_new);
}

/**
 * Get file status
 */
error_code_t vfs_stat(const char* path, vfs_stat_t* stat) {
    if (!path || !stat) {
        return ERR_INVALID_ARG;
    }
    
    vfs_mount_t* mount;
    char resolved_path[256];
    error_code_t err = vfs_resolve_path(path, &mount, resolved_path);
    if (err != ERR_OK || !mount || !mount->fs) {
        return err != ERR_OK ? err : ERR_NOT_FOUND;
    }
    
    if (!mount->fs->stat) {
        return ERR_NOT_SUPPORTED;
    }
    
    return mount->fs->stat(mount->fs, resolved_path, stat);
}

/**
 * Unmount filesystem
 */
error_code_t vfs_unmount(const char* mountpoint) {
    (void)mountpoint;
    // TODO: Implement unmount
    return ERR_NOT_SUPPORTED;
}

