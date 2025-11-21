/**
 * @file ext4.h
 * @brief ext4 filesystem interface
 */

#ifndef KERNEL_FS_EXT4_H
#define KERNEL_FS_EXT4_H

#include "../types.h"
#include "../errors.h"
#include "block.h"

// ext4 magic number
#define EXT4_SUPER_MAGIC 0xEF53

// ext4 superblock structure (simplified)
typedef struct {
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_cluster_size;
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint32_t mount_time;
    uint32_t write_time;
    uint16_t mount_count;
    uint16_t max_mount_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t def_resuid;
    uint16_t def_resgid;
    uint32_t first_ino;
    uint16_t inode_size;
    uint16_t block_group_nr;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint8_t uuid[16];
    char volume_name[16];
    char last_mounted[64];
    uint32_t algorithm_usage_bitmap;
    uint8_t prealloc_blocks;
    uint8_t prealloc_dir_blocks;
    uint16_t reserved_gdt_blocks;
    uint8_t journal_uuid[16];
    uint32_t journal_inum;
    uint32_t journal_dev;
    uint32_t last_orphan;
    uint32_t hash_seed[4];
    uint8_t def_hash_version;
    uint8_t jnl_backup_type;
    uint16_t group_desc_size;
    uint32_t default_mount_opts;
    uint32_t first_meta_bg;
    uint32_t mkfs_time;
    uint32_t jnl_blocks[17];
} __attribute__((packed)) ext4_superblock_t;

// ext4 inode structure (simplified)
typedef struct {
    uint16_t mode;
    uint16_t uid;
    uint32_t size_lo;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks_lo;
    uint32_t flags;
    uint32_t osd1;
    uint32_t block[15];
    uint32_t generation;
    uint32_t file_acl_lo;
    uint32_t size_hi;
    uint32_t obso_faddr;
    uint16_t blocks_hi;
    uint16_t file_acl_hi;
    uint16_t uid_hi;
    uint16_t gid_hi;
    uint32_t checksum_lo;
} __attribute__((packed)) ext4_inode_t;

// ext4 directory entry structure
typedef struct {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[];
} __attribute__((packed)) ext4_dir_entry_t;

// ext4 filesystem structure
typedef struct {
    block_device_t* device;
    ext4_superblock_t superblock;
    uint32_t block_size;
    uint32_t inode_size;
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint32_t group_count;
    bool initialized;
} ext4_fs_t;

// ext4 functions
error_code_t ext4_init(block_device_t* device, ext4_fs_t* fs);
error_code_t ext4_mount(ext4_fs_t* fs, const char* mountpoint);
error_code_t ext4_unmount(ext4_fs_t* fs);
error_code_t ext4_read_inode(ext4_fs_t* fs, uint32_t inode_num, ext4_inode_t* inode);
error_code_t ext4_find_file(ext4_fs_t* fs, uint32_t parent_inode, const char* name, uint32_t* inode_num);
error_code_t ext4_read_file(ext4_fs_t* fs, uint32_t inode_num, void* buffer, size_t offset, size_t count, size_t* bytes_read);
error_code_t ext4_read_dir(ext4_fs_t* fs, uint32_t inode_num, ext4_dir_entry_t* entries, size_t max_entries, size_t* entry_count);

#endif // KERNEL_FS_EXT4_H

