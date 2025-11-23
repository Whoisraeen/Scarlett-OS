/**
 * @file vfs.h
 * @brief Virtual File System interface
 * 
 * Abstract filesystem layer that supports multiple filesystem types.
 */

#ifndef KERNEL_FS_VFS_H
#define KERNEL_FS_VFS_H

#include "../types.h"
#include "../errors.h"

// File modes
#define VFS_MODE_READ    (1 << 0)
#define VFS_MODE_WRITE   (1 << 1)
#define VFS_MODE_EXEC    (1 << 2)
#define VFS_MODE_CREATE  (1 << 3)
#define VFS_MODE_APPEND  (1 << 4)
#define VFS_MODE_TRUNC   (1 << 5)

// File types
typedef enum {
    VFS_TYPE_FILE,
    VFS_TYPE_DIRECTORY,
    VFS_TYPE_SYMLINK,
    VFS_TYPE_DEVICE,
    VFS_TYPE_UNKNOWN
} vfs_file_type_t;

// File descriptor
typedef int fd_t;

// Inode number
typedef uint64_t ino_t;

// File information structure
typedef struct {
    ino_t ino;              // Inode number
    vfs_file_type_t type;   // File type
    size_t size;            // File size
    uint64_t mode;          // File permissions
    uint64_t uid;           // Owner UID
    uint64_t gid;           // Owner GID
    uint64_t atime;         // Access time
    uint64_t mtime;         // Modify time
    uint64_t ctime;         // Change time
} vfs_stat_t;

// Directory entry
typedef struct {
    ino_t ino;              // Inode number
    char name[256];         // File name
    vfs_file_type_t type;   // File type
} vfs_dirent_t;

// Filesystem operations structure
typedef struct vfs_filesystem {
    const char* name;       // Filesystem name (e.g., "fat32")
    
    // Mount/unmount
    error_code_t (*mount)(struct vfs_filesystem* fs, const char* device, const char* mountpoint);
    error_code_t (*unmount)(struct vfs_filesystem* fs);
    
    // File operations
    error_code_t (*open)(struct vfs_filesystem* fs, const char* path, uint64_t flags, fd_t* fd);
    error_code_t (*close)(struct vfs_filesystem* fs, fd_t fd);
    error_code_t (*read)(struct vfs_filesystem* fs, fd_t fd, void* buf, size_t count, size_t* bytes_read);
    error_code_t (*write)(struct vfs_filesystem* fs, fd_t fd, const void* buf, size_t count, size_t* bytes_written);
    error_code_t (*seek)(struct vfs_filesystem* fs, fd_t fd, int64_t offset, int whence);
    error_code_t (*tell)(struct vfs_filesystem* fs, fd_t fd, size_t* position);
    
    // Directory operations
    error_code_t (*mkdir)(struct vfs_filesystem* fs, const char* path);
    error_code_t (*rmdir)(struct vfs_filesystem* fs, const char* path);
    error_code_t (*opendir)(struct vfs_filesystem* fs, const char* path, fd_t* fd);
    error_code_t (*readdir)(struct vfs_filesystem* fs, fd_t fd, vfs_dirent_t* entry);
    error_code_t (*closedir)(struct vfs_filesystem* fs, fd_t fd);
    
    // File management
    error_code_t (*unlink)(struct vfs_filesystem* fs, const char* path);
    error_code_t (*rename)(struct vfs_filesystem* fs, const char* oldpath, const char* newpath);
    error_code_t (*stat)(struct vfs_filesystem* fs, const char* path, vfs_stat_t* stat);
    
    // Filesystem-specific data
    void* private_data;
    
    // Linked list
    struct vfs_filesystem* next;
} vfs_filesystem_t;

// Mount point
typedef struct vfs_mount {
    const char* mountpoint;      // Mount point path (e.g., "/")
    vfs_filesystem_t* fs;         // Filesystem
    struct vfs_mount* next;       // Linked list
} vfs_mount_t;

// VFS functions
error_code_t vfs_init(void);
error_code_t vfs_register_filesystem(vfs_filesystem_t* fs);
error_code_t vfs_mount(const char* device, const char* mountpoint, const char* fstype);
error_code_t vfs_unmount(const char* mountpoint);

// File operations
error_code_t vfs_open(const char* path, uint64_t flags, fd_t* fd);
error_code_t vfs_close(fd_t fd);
error_code_t vfs_read(fd_t fd, void* buf, size_t count, size_t* bytes_read);
error_code_t vfs_write(fd_t fd, const void* buf, size_t count, size_t* bytes_written);
error_code_t vfs_seek(fd_t fd, int64_t offset, int whence);
error_code_t vfs_tell(fd_t fd, size_t* position);

// Directory operations
error_code_t vfs_mkdir(const char* path);
error_code_t vfs_rmdir(const char* path);
error_code_t vfs_opendir(const char* path, fd_t* fd);
error_code_t vfs_readdir(fd_t fd, vfs_dirent_t* entry);
error_code_t vfs_closedir(fd_t fd);

// File management
error_code_t vfs_unlink(const char* path);
error_code_t vfs_rename(const char* oldpath, const char* newpath);
error_code_t vfs_stat(const char* path, vfs_stat_t* stat);

// Path resolution
error_code_t vfs_resolve_path(const char* path, vfs_mount_t** mount, char* resolved_path);

#endif // KERNEL_FS_VFS_H

