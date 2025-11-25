/**
 * @file sfs.h
 * @brief Simple File System (SFS) definitions
 */

#ifndef KERNEL_FS_SFS_H
#define KERNEL_FS_SFS_H

#include "../types.h"
#include "../fs/vfs.h"
#include "../fs/block.h"

#define SFS_MAGIC 0x53465331 // "SFS1"
#define SFS_DEFAULT_BLOCK_SIZE 4096
#define SFS_FILENAME_MAX 64

// SFS Superblock
typedef struct {
    uint32_t magic;             // Magic number
    uint32_t block_size;        // Block size in bytes
    uint32_t blocks_count;      // Total blocks
    uint32_t inodes_count;      // Total inodes
    uint32_t free_blocks;       // Free blocks count
    uint32_t free_inodes;       // Free inodes count
    uint32_t inode_bitmap_block;// Block containing inode bitmap
    uint32_t block_bitmap_block;// Block containing block bitmap
    uint32_t inode_table_block; // Start block of inode table
    uint32_t data_block_start;  // Start block of data
    uint32_t root_inode;        // Root directory inode index
    uint32_t padding[5];
} __attribute__((packed)) sfs_superblock_t;

// SFS Inode
typedef struct {
    uint32_t type;              // File type (VFS_TYPE_*)
    uint32_t size;              // File size in bytes
    uint32_t uid;               // Owner UID
    uint32_t gid;               // Owner GID
    uint32_t mode;              // Permissions
    uint32_t atime;             // Access time
    uint32_t mtime;             // Modify time
    uint32_t ctime;             // Create time
    uint32_t blocks[12];        // Direct data blocks
    uint32_t indirect_block;    // Single indirect block
    uint32_t padding[3];
} __attribute__((packed)) sfs_inode_t;

// SFS Directory Entry
typedef struct {
    uint32_t inode;             // Inode number (0 = empty)
    char name[SFS_FILENAME_MAX];// Filename
} __attribute__((packed)) sfs_dirent_t;

// SFS Filesystem structure (in-memory)
typedef struct {
    block_device_t* device;
    sfs_superblock_t superblock;
    bool initialized;
    // Caches could go here
} sfs_fs_t;

// Functions
error_code_t sfs_init(block_device_t* device, sfs_fs_t* fs);
error_code_t sfs_format(block_device_t* device);
error_code_t sfs_mount(sfs_fs_t* fs, const char* mountpoint);
error_code_t sfs_unmount(sfs_fs_t* fs);

// VFS operations
error_code_t sfs_open(vfs_filesystem_t* fs, const char* path, uint64_t flags, fd_t* fd);
error_code_t sfs_close(vfs_filesystem_t* fs, fd_t fd);
error_code_t sfs_read(vfs_filesystem_t* fs, fd_t fd, void* buf, size_t count, size_t* bytes_read);
error_code_t sfs_write(vfs_filesystem_t* fs, fd_t fd, const void* buf, size_t count, size_t* bytes_written);
error_code_t sfs_seek(vfs_filesystem_t* fs, fd_t fd, int64_t offset, int whence);
error_code_t sfs_tell(vfs_filesystem_t* fs, fd_t fd, size_t* position);
error_code_t sfs_mkdir(vfs_filesystem_t* fs, const char* path);
error_code_t sfs_rmdir(vfs_filesystem_t* fs, const char* path);
error_code_t sfs_opendir(vfs_filesystem_t* fs, const char* path, fd_t* fd);
error_code_t sfs_readdir(vfs_filesystem_t* fs, fd_t fd, vfs_dirent_t* entry);
error_code_t sfs_closedir(vfs_filesystem_t* fs, fd_t fd);
error_code_t sfs_unlink(vfs_filesystem_t* fs, const char* path);
error_code_t sfs_stat(vfs_filesystem_t* fs, const char* path, vfs_stat_t* stat);

#endif // KERNEL_FS_SFS_H
