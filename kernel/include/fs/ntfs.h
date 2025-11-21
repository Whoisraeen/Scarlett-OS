/**
 * @file ntfs.h
 * @brief NTFS filesystem interface (read-only)
 */

#ifndef KERNEL_FS_NTFS_H
#define KERNEL_FS_NTFS_H

#include "../types.h"
#include "../errors.h"
#include "block.h"

// NTFS magic signatures
#define NTFS_BOOT_SECTOR_MAGIC "NTFS    "

// NTFS boot sector structure
typedef struct {
    uint8_t jump[3];
    char oem_id[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t media_descriptor;
    uint16_t unused1;
    uint16_t sectors_per_track;
    uint16_t number_of_heads;
    uint32_t hidden_sectors;
    uint8_t unused2[8];
    uint64_t total_sectors;
    uint64_t mft_cluster;
    uint64_t mft_mirror_cluster;
    int8_t clusters_per_mft_record;
    uint8_t clusters_per_index_record;
    uint64_t volume_serial;
    uint32_t checksum;
    uint8_t boot_code[426];
    uint16_t boot_signature;
} __attribute__((packed)) ntfs_boot_sector_t;

// NTFS MFT record header
typedef struct {
    uint32_t magic;  // "FILE"
    uint16_t update_sequence_offset;
    uint16_t update_sequence_size;
    uint64_t logfile_sequence_number;
    uint16_t sequence_number;
    uint16_t link_count;
    uint16_t attribute_offset;
    uint16_t flags;
    uint32_t real_size;
    uint32_t allocated_size;
    uint64_t base_record;
    uint16_t next_attribute_id;
    uint16_t padding;
    uint32_t record_number;
} __attribute__((packed)) ntfs_mft_record_t;

// NTFS attribute types
#define NTFS_ATTR_STANDARD_INFORMATION 0x10
#define NTFS_ATTR_FILE_NAME             0x30
#define NTFS_ATTR_DATA                  0x80

// NTFS filesystem structure
typedef struct {
    block_device_t* device;
    ntfs_boot_sector_t boot_sector;
    uint32_t bytes_per_sector;
    uint32_t bytes_per_cluster;
    uint64_t mft_cluster;
    uint64_t mft_mirror_cluster;
    uint32_t mft_record_size;
    bool initialized;
} ntfs_fs_t;

// NTFS functions (read-only)
error_code_t ntfs_init(block_device_t* device, ntfs_fs_t* fs);
error_code_t ntfs_mount(ntfs_fs_t* fs, const char* mountpoint);
error_code_t ntfs_unmount(ntfs_fs_t* fs);
error_code_t ntfs_read_mft_record(ntfs_fs_t* fs, uint64_t record_number, void* buffer);
error_code_t ntfs_find_file(ntfs_fs_t* fs, const char* path, uint64_t* mft_record);
error_code_t ntfs_read_file(ntfs_fs_t* fs, uint64_t mft_record, void* buffer, size_t offset, size_t count, size_t* bytes_read);

#endif // KERNEL_FS_NTFS_H

