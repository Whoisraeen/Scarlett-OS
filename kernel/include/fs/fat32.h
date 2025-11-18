/**
 * @file fat32.h
 * @brief FAT32 filesystem structures and interface
 */

#ifndef KERNEL_FS_FAT32_H
#define KERNEL_FS_FAT32_H

#include "../types.h"
#include "../errors.h"
#include "vfs.h"
#include "block.h"

// FAT32 Boot Sector structure
typedef struct __attribute__((packed)) {
    uint8_t jump[3];                    // Jump instruction
    char oem_name[8];                    // OEM name
    uint16_t bytes_per_sector;           // Bytes per sector (usually 512)
    uint8_t sectors_per_cluster;         // Sectors per cluster
    uint16_t reserved_sectors;          // Reserved sectors
    uint8_t num_fats;                    // Number of FATs
    uint16_t root_entries;               // Root directory entries (FAT12/16)
    uint16_t total_sectors_16;           // Total sectors (FAT12/16)
    uint8_t media_type;                  // Media type
    uint16_t sectors_per_fat_16;         // Sectors per FAT (FAT12/16)
    uint16_t sectors_per_track;          // Sectors per track
    uint16_t num_heads;                  // Number of heads
    uint32_t hidden_sectors;            // Hidden sectors
    uint32_t total_sectors_32;           // Total sectors (FAT32)
    uint32_t sectors_per_fat_32;         // Sectors per FAT (FAT32)
    uint16_t flags;                      // Flags
    uint16_t version;                    // Version
    uint32_t root_cluster;               // Root cluster number
    uint16_t fs_info_sector;             // FS info sector
    uint16_t backup_boot_sector;         // Backup boot sector
    uint8_t reserved[12];                // Reserved
    uint8_t drive_number;                // Drive number
    uint8_t reserved1;                   // Reserved
    uint8_t boot_signature;              // Boot signature
    uint32_t volume_id;                  // Volume ID
    char volume_label[11];               // Volume label
    char fs_type[8];                     // File system type
    uint8_t boot_code[420];              // Boot code
    uint16_t boot_signature_end;         // Boot signature (0xAA55)
} fat32_boot_sector_t;

// FAT32 Directory Entry
typedef struct __attribute__((packed)) {
    char name[11];                       // 8.3 filename
    uint8_t attributes;                  // File attributes
    uint8_t reserved;                    // Reserved
    uint8_t creation_time_tenths;         // Creation time (tenths of second)
    uint16_t creation_time;              // Creation time
    uint16_t creation_date;              // Creation date
    uint16_t access_date;                // Access date
    uint16_t cluster_high;               // High 16 bits of cluster
    uint16_t modification_time;         // Modification time
    uint16_t modification_date;         // Modification date
    uint16_t cluster_low;                // Low 16 bits of cluster
    uint32_t file_size;                  // File size
} fat32_dir_entry_t;

// FAT32 file attributes
#define FAT32_ATTR_READ_ONLY  0x01
#define FAT32_ATTR_HIDDEN     0x02
#define FAT32_ATTR_SYSTEM     0x04
#define FAT32_ATTR_VOLUME_ID  0x08
#define FAT32_ATTR_DIRECTORY  0x10
#define FAT32_ATTR_ARCHIVE    0x20
#define FAT32_ATTR_LONG_NAME  0x0F

// FAT32 cluster values
#define FAT32_CLUSTER_FREE    0x00000000
#define FAT32_CLUSTER_RESERVED_MIN 0x00000001
#define FAT32_CLUSTER_RESERVED_MAX 0x0FFFFFF6
#define FAT32_CLUSTER_BAD     0x0FFFFFF7
#define FAT32_CLUSTER_EOF_MIN 0x0FFFFFF8
#define FAT32_CLUSTER_EOF_MAX 0x0FFFFFFF

// FAT32 filesystem structure
typedef struct {
    block_device_t* device;              // Block device
    fat32_boot_sector_t boot_sector;     // Boot sector
    uint32_t sectors_per_cluster;        // Sectors per cluster
    uint32_t bytes_per_cluster;          // Bytes per cluster
    uint32_t fat_start_sector;           // FAT start sector
    uint32_t fat_size_sectors;           // FAT size in sectors
    uint32_t data_start_sector;          // Data area start sector
    uint32_t root_cluster;               // Root directory cluster
    uint32_t total_clusters;             // Total clusters
    uint8_t* fat_cache;                  // FAT cache (one sector)
    uint32_t fat_cache_sector;           // Cached FAT sector
} fat32_fs_t;

// FAT32 functions
error_code_t fat32_init(block_device_t* device, fat32_fs_t* fs);
error_code_t fat32_mount(fat32_fs_t* fs, const char* mountpoint);
error_code_t fat32_read_cluster(fat32_fs_t* fs, uint32_t cluster, void* buffer);
error_code_t fat32_write_cluster(fat32_fs_t* fs, uint32_t cluster, const void* buffer);
uint32_t fat32_get_next_cluster(fat32_fs_t* fs, uint32_t cluster);
error_code_t fat32_set_next_cluster(fat32_fs_t* fs, uint32_t cluster, uint32_t next);
uint32_t fat32_alloc_cluster(fat32_fs_t* fs);
error_code_t fat32_free_cluster(fat32_fs_t* fs, uint32_t cluster);
error_code_t fat32_read_dir(fat32_fs_t* fs, uint32_t cluster, fat32_dir_entry_t* entries, size_t max_entries);
error_code_t fat32_find_file(fat32_fs_t* fs, const char* path, fat32_dir_entry_t* entry);

// File operations
error_code_t fat32_file_open(fat32_fs_t* fs, const char* path, uint64_t flags, fd_t* fd);
error_code_t fat32_file_close(fat32_fs_t* fs, fd_t fd);
error_code_t fat32_file_read(fat32_fs_t* fs, fd_t fd, void* buf, size_t count, size_t* bytes_read);
error_code_t fat32_file_write(fat32_fs_t* fs, fd_t fd, const void* buf, size_t count, size_t* bytes_written);
error_code_t fat32_file_seek(fat32_fs_t* fs, fd_t fd, int64_t offset, int whence);
error_code_t fat32_file_tell(fat32_fs_t* fs, fd_t fd, size_t* position);

// File creation/deletion
error_code_t fat32_create_file(fat32_fs_t* fs, const char* path, fat32_dir_entry_t* entry);
error_code_t fat32_delete_file(fat32_fs_t* fs, const char* path);

#endif // KERNEL_FS_FAT32_H

