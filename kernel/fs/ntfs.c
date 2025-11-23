/**
 * @file ntfs.c
 * @brief NTFS filesystem implementation (read-only)
 */

#include "../include/fs/ntfs.h"
#include "../include/fs/block.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/mm/heap.h"

/**
 * Read NTFS boot sector
 */
static error_code_t ntfs_read_boot_sector(block_device_t* device, ntfs_boot_sector_t* boot) {
    if (!device || !boot) {
        return ERR_INVALID_ARG;
    }
    
    // Read first sector
    error_code_t err = block_device_read(device, 0, boot);
    if (err != ERR_OK) {
        return err;
    }
    
    // Verify magic signature
    if (memcmp(boot->oem_id, NTFS_BOOT_SECTOR_MAGIC, 8) != 0) {
        kerror("NTFS: Invalid boot sector signature\n");
        return ERR_INVALID_ELF;
    }
    
    // Verify boot signature
    if (boot->boot_signature != 0xAA55) {
        kerror("NTFS: Invalid boot signature\n");
        return ERR_INVALID_ELF;
    }
    
    return ERR_OK;
}

/**
 * Initialize NTFS filesystem
 */
error_code_t ntfs_init(block_device_t* device, ntfs_fs_t* fs) {
    if (!device || !fs) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("Initializing NTFS filesystem on device %s...\n", device->name);
    
    // Read boot sector
    error_code_t err = ntfs_read_boot_sector(device, &fs->boot_sector);
    if (err != ERR_OK) {
        return err;
    }
    
    // Calculate filesystem parameters
    fs->device = device;
    fs->bytes_per_sector = fs->boot_sector.bytes_per_sector;
    fs->bytes_per_cluster = fs->bytes_per_sector * fs->boot_sector.sectors_per_cluster;
    fs->mft_cluster = fs->boot_sector.mft_cluster;
    fs->mft_mirror_cluster = fs->boot_sector.mft_mirror_cluster;
    
    // Calculate MFT record size
    if (fs->boot_sector.clusters_per_mft_record < 0) {
        // Negative value means size in bytes (2^abs(value))
        fs->mft_record_size = 1 << (-fs->boot_sector.clusters_per_mft_record);
    } else {
        fs->mft_record_size = fs->boot_sector.clusters_per_mft_record * fs->bytes_per_cluster;
    }
    
    fs->initialized = true;
    
    kinfo("NTFS: Sector size: %u, Cluster size: %u, MFT record size: %u\n",
          fs->bytes_per_sector, fs->bytes_per_cluster, fs->mft_record_size);
    kinfo("NTFS: MFT cluster: %llu, Mirror cluster: %llu\n",
          fs->mft_cluster, fs->mft_mirror_cluster);
    
    return ERR_OK;
}

/**
 * Mount NTFS filesystem
 */
error_code_t ntfs_mount(ntfs_fs_t* fs, const char* mountpoint) {
    if (!fs || !fs->initialized) {
        return ERR_INVALID_STATE;
    }
    
    (void)mountpoint;
    kinfo("NTFS: Mounted at %s (read-only)\n", mountpoint);
    return ERR_OK;
}

/**
 * Unmount NTFS filesystem
 */
error_code_t ntfs_unmount(ntfs_fs_t* fs) {
    if (!fs) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("NTFS: Unmounting...\n");
    fs->initialized = false;
    return ERR_OK;
}

/**
 * Read MFT record
 */
error_code_t ntfs_read_mft_record(ntfs_fs_t* fs, uint64_t record_number, void* buffer) {
    if (!fs || !fs->initialized || !buffer) {
        return ERR_INVALID_ARG;
    }
    
    // Calculate cluster and sector
    uint64_t mft_cluster = fs->mft_cluster + (record_number * fs->mft_record_size) / fs->bytes_per_cluster;
    uint64_t mft_sector = mft_cluster * fs->boot_sector.sectors_per_cluster;
    
    // Read sectors containing MFT record
    uint32_t sectors_per_record = (fs->mft_record_size + fs->bytes_per_sector - 1) / fs->bytes_per_sector;
    
    uint8_t* sector_buffer = (uint8_t*)kmalloc(sectors_per_record * fs->bytes_per_sector);
    if (!sector_buffer) {
        return ERR_OUT_OF_MEMORY;
    }
    
    for (uint32_t i = 0; i < sectors_per_record; i++) {
        error_code_t err = block_device_read(fs->device, mft_sector + i, sector_buffer + (i * fs->bytes_per_sector));
        if (err != ERR_OK) {
            kfree(sector_buffer);
            return err;
        }
    }
    
    // Copy MFT record
    memcpy(buffer, sector_buffer, fs->mft_record_size);
    kfree(sector_buffer);
    
    // Verify magic
    ntfs_mft_record_t* record = (ntfs_mft_record_t*)buffer;
    if (record->magic != 0x454C4946) {  // "FILE"
        return ERR_INVALID_ELF;
    }
    
    return ERR_OK;
}

// Helper: Parse $FILE_NAME attribute to get name
// Returns 0 on match, 1 on mismatch, -1 on error
static int ntfs_compare_filename(uint8_t* attr_ptr, const char* name) {
    // $FILE_NAME attribute structure:
    // Header (standard)
    // 0x00: Parent reference (8 bytes)
    // 0x08: Creation time (8 bytes)
    // 0x10: Modification time (8 bytes)
    // 0x18: MFT change time (8 bytes)
    // 0x20: Access time (8 bytes)
    // 0x28: Allocated size (8 bytes)
    // 0x30: Real size (8 bytes)
    // 0x38: Flags (4 bytes)
    // 0x3C: Reparse value (4 bytes)
    // 0x40: Name length (1 byte)
    // 0x41: Namespace (1 byte)
    // 0x42: Name (unicode)
    
    uint8_t resident = attr_ptr[8];
    if (resident != 0) return -1; // Must be resident
    
    uint16_t content_offset = *(uint16_t*)(attr_ptr + 20);
    uint8_t* content = attr_ptr + content_offset;
    
    uint8_t name_len = content[0x40];
    uint16_t* name_utf16 = (uint16_t*)(content + 0x42);
    
    // Simple conversion/comparison for ASCII/UTF-8 (ignoring wide chars > 255 for now)
    size_t input_len = strlen(name);
    if (input_len != name_len) return 1;
    
    for (size_t i = 0; i < name_len; i++) {
        // Simple case-insensitive check? NTFS is case-sensitive but Windows treats it case-insensitive.
        // Let's do exact match first.
        char c = (char)(name_utf16[i] & 0xFF);
        if (c != name[i]) return 1;
    }
    
    return 0;
}

// Helper: Read index root/allocation for directory listing
// returns found MFT record or 0
static uint64_t ntfs_search_directory(ntfs_fs_t* fs, uint64_t dir_mft, const char* name) {
    // This is still complex. Directories in NTFS are B+ trees.
    // We need to parse $INDEX_ROOT and potentially $INDEX_ALLOCATION.
    
    // Simplified approach for now:
    // 1. Read MFT of directory
    // 2. Look for $INDEX_ROOT (0x90)
    // 3. Scan entries in $INDEX_ROOT
    
    uint8_t* mft_buffer = (uint8_t*)kmalloc(fs->mft_record_size);
    if (!mft_buffer) return 0;
    
    if (ntfs_read_mft_record(fs, dir_mft, mft_buffer) != ERR_OK) {
        kfree(mft_buffer);
        return 0;
    }
    
    ntfs_mft_record_t* record = (ntfs_mft_record_t*)mft_buffer;
    uint16_t attr_offset = record->attribute_offset;
    uint8_t* attr_ptr = mft_buffer + attr_offset;
    
    while (attr_offset < fs->mft_record_size) {
        uint32_t attr_type = *(uint32_t*)attr_ptr;
        uint32_t attr_length = *(uint32_t*)(attr_ptr + 4);
        
        if (attr_type == 0xFFFFFFFF) break;
        
        if (attr_type == 0x90) { // $INDEX_ROOT
            uint8_t resident = attr_ptr[8];
            if (resident == 0) {
                uint16_t content_offset = *(uint16_t*)(attr_ptr + 20);
                uint8_t* content = attr_ptr + content_offset;
                
                // Index Root Header:
                // 0x00: Attribute Type (4)
                // 0x04: Collation Rule (4)
                // 0x08: Index Allocation Entry Size (4)
                // 0x0C: Clusters per Index Record (1)
                // 0x10: Padding (3)
                // 0x10: Node Header (16 bytes)
                
                uint8_t* node_header = content + 16;
                uint32_t entries_offset = *(uint32_t*)(node_header + 0);
                uint32_t entries_size = *(uint32_t*)(node_header + 4);
                
                uint8_t* entry_ptr = node_header + entries_offset;
                uint8_t* entry_end = node_header + entries_size;
                
                while (entry_ptr < entry_end) {
                    // Index Entry:
                    // 0x00: MFT Reference (8)
                    // 0x08: Entry Length (2)
                    // 0x0A: Stream Length (2)
                    // 0x0C: Flags (1)
                    // 0x10: Stream...
                    
                    uint64_t mft_ref = *(uint64_t*)entry_ptr;
                    uint16_t entry_len = *(uint16_t*)(entry_ptr + 8);
                    uint16_t stream_len = *(uint16_t*)(entry_ptr + 0x0A);
                    uint8_t flags = entry_ptr[0x0C];
                    
                    if (flags & 2) break; // Last entry
                    
                    // Stream contains $FILE_NAME attribute
                    if (stream_len > 0) {
                        uint8_t* file_name_attr = entry_ptr + 16;
                        // The stream is actually the $FILE_NAME attribute content directly
                        // file_name_attr points to: Parent Ref(8), Creation(8)...
                        
                        uint8_t name_len = file_name_attr[64];
                        uint16_t* name_utf16 = (uint16_t*)(file_name_attr + 66);
                        
                        // Compare name
                        size_t input_len = strlen(name);
                        if (input_len == name_len) {
                            bool match = true;
                            for (size_t i = 0; i < name_len; i++) {
                                if ((char)(name_utf16[i] & 0xFF) != name[i]) {
                                    match = false;
                                    break;
                                }
                            }
                            if (match) {
                                kfree(mft_buffer);
                                return mft_ref & 0xFFFFFFFFFFFF; // Lower 48 bits are MFT record
                            }
                        }
                    }
                    
                    entry_ptr += entry_len;
                }
            }
        }
        
        // Move to next attribute
        attr_offset += attr_length;
        attr_ptr += attr_length;
    }
    
    kfree(mft_buffer);
    return 0;
}

/**
 * Find file in NTFS (Full path parsing)
 */
error_code_t ntfs_find_file(ntfs_fs_t* fs, const char* path, uint64_t* mft_record) {
    if (!fs || !fs->initialized || !path || !mft_record) {
        return ERR_INVALID_ARG;
    }
    
    // Start from root directory (MFT record 5)
    uint64_t current_record = 5;
    
    // Copy path to modify it
    char path_buf[256];
    strncpy(path_buf, path, 255);
    
    char* component = strtok(path_buf, "/");
    while (component) {
        uint64_t next_record = ntfs_search_directory(fs, current_record, component);
        if (next_record == 0) {
            return ERR_NOT_FOUND;
        }
        current_record = next_record;
        component = strtok(NULL, "/");
    }
    
    *mft_record = current_record;
    return ERR_OK;
}

/**
 * Read file data from NTFS
 */
error_code_t ntfs_read_file(ntfs_fs_t* fs, uint64_t mft_record, void* buffer, size_t offset, size_t count, size_t* bytes_read) {
    if (!fs || !fs->initialized || !buffer || !bytes_read) {
        return ERR_INVALID_ARG;
    }
    
    // Read MFT record
    uint8_t* mft_buffer = (uint8_t*)kmalloc(fs->mft_record_size);
    if (!mft_buffer) {
        return ERR_OUT_OF_MEMORY;
    }
    
    error_code_t err = ntfs_read_mft_record(fs, mft_record, mft_buffer);
    if (err != ERR_OK) {
        kfree(mft_buffer);
        return err;
    }
    
    // Parse attributes and read data runs
    ntfs_mft_record_t* record = (ntfs_mft_record_t*)mft_buffer;
    
    // Find $DATA attribute (type 0x80)
    uint16_t attr_offset = record->attribute_offset;
    uint8_t* attr_ptr = mft_buffer + attr_offset;
    
    while (attr_offset < fs->mft_record_size) {
        uint32_t attr_type = *(uint32_t*)attr_ptr;
        uint32_t attr_length = *(uint32_t*)(attr_ptr + 4);
        
        if (attr_type == 0xFFFFFFFF) {
            // End of attributes
            break;
        }
        
        if (attr_type == NTFS_ATTR_DATA) {
            // Found $DATA attribute
            uint8_t non_resident = attr_ptr[8];
            
            if (non_resident == 0) {
                // Resident attribute - data is in the attribute itself
                uint16_t data_offset = *(uint16_t*)(attr_ptr + 20);
                uint32_t data_size = *(uint32_t*)(attr_ptr + 16);
                
                if (offset < data_size) {
                    size_t to_copy = count;
                    if (offset + to_copy > data_size) {
                        to_copy = data_size - offset;
                    }
                    
                    memcpy(buffer, attr_ptr + data_offset + offset, to_copy);
                    *bytes_read = to_copy;
                    kfree(mft_buffer);
                    return ERR_OK;
                }
            } else {
                // Non-resident attribute - parse data runs
                uint16_t data_run_offset = *(uint16_t*)(attr_ptr + 32);
                uint64_t data_size = *(uint64_t*)(attr_ptr + 48);
                
                if (offset >= data_size) {
                    *bytes_read = 0;
                    kfree(mft_buffer);
                    return ERR_OK;
                }
                
                size_t to_read = count;
                if (offset + to_read > data_size) {
                    to_read = data_size - offset;
                }
                
                // Parse data runs (compressed format)
                uint8_t* run_ptr = attr_ptr + data_run_offset;
                uint64_t current_vcn = 0;  // Virtual cluster number
                uint64_t current_lcn = 0;  // Logical cluster number
                uint64_t bytes_read_so_far = 0;
                uint8_t* output_ptr = (uint8_t*)buffer;
                
                while (*run_ptr != 0 && bytes_read_so_far < offset + to_read) {
                    // Data run format: [length][offset]
                    uint8_t length_bytes = (*run_ptr) & 0x0F;
                    uint8_t offset_bytes = ((*run_ptr) >> 4) & 0x0F;
                    run_ptr++;
                    
                    // Read length
                    uint64_t run_length = 0;
                    for (uint8_t i = 0; i < length_bytes; i++) {
                        run_length |= ((uint64_t)*run_ptr++) << (i * 8);
                    }
                    
                    // Read offset (signed, relative to previous)
                    int64_t run_offset = 0;
                    for (uint8_t i = 0; i < offset_bytes; i++) {
                        run_offset |= ((uint64_t)*run_ptr++) << (i * 8);
                    }
                    
                    // Sign extend if necessary
                    if (offset_bytes > 0 && (run_offset & (1ULL << (offset_bytes * 8 - 1)))) {
                        // If top bit of read bytes is set, sign extend
                         for (uint8_t i = offset_bytes; i < 8; i++) {
                            run_offset |= 0xFFULL << (i * 8);
                        }
                    }
                    
                    current_lcn += run_offset;
                    current_vcn += run_length;
                    
                    // Calculate if this run contains data we need
                    uint64_t run_start_byte = (current_vcn - run_length) * fs->bytes_per_cluster; // Fix: VCN is end? No, it's start usually, but here accumulated.
                    // Standard says VCNs are sequential.
                    // Wait, VCN accumulation above: current_vcn += run_length. So current_vcn is END of run.
                    // Start of run is current_vcn - run_length.
                    uint64_t run_start_vcn = current_vcn - run_length;
                    run_start_byte = run_start_vcn * fs->bytes_per_cluster;
                    uint64_t run_end_byte = current_vcn * fs->bytes_per_cluster;
                    
                    if (offset < run_end_byte && offset + to_read > run_start_byte) {
                        // This run contains data we need
                        uint64_t read_start = (offset > run_start_byte) ? offset : run_start_byte;
                        uint64_t read_end = (offset + to_read < run_end_byte) ? offset + to_read : run_end_byte;
                        uint64_t read_size = read_end - read_start;
                        
                        // Calculate cluster offset
                        uint64_t cluster_offset = (read_start - run_start_byte) / fs->bytes_per_cluster;
                        uint64_t sector_offset = (read_start - run_start_byte) % fs->bytes_per_cluster;
                        uint64_t sector = (current_lcn + cluster_offset) * fs->boot_sector.sectors_per_cluster + (sector_offset / fs->bytes_per_sector);
                        
                        // Read data
                        // Use a temporary sector buffer to handle alignment if needed, or read directly if aligned
                        // For simplicity/safety with unaligned counts, we use a sector buffer.
                        uint8_t* sector_temp = (uint8_t*)kmalloc(fs->bytes_per_sector);
                         if (!sector_temp) {
                            kfree(mft_buffer);
                            return ERR_OUT_OF_MEMORY;
                        }
                        
                        uint64_t bytes_remaining_in_run = read_size;
                        uint64_t current_sector = sector;
                        uint8_t* run_output_ptr = output_ptr; // local ptr for this run
                        
                        // Read sector by sector
                        uint64_t sector_offset_in_run = sector_offset; // offset in first sector
                        
                        while (bytes_remaining_in_run > 0) {
                            if (block_device_read(fs->device, current_sector, sector_temp) != ERR_OK) {
                                kfree(sector_temp); kfree(mft_buffer); return ERR_IO_ERROR;
                            }
                            
                            size_t copy_len = fs->bytes_per_sector - sector_offset_in_run;
                            if (copy_len > bytes_remaining_in_run) copy_len = bytes_remaining_in_run;
                            
                            memcpy(run_output_ptr, sector_temp + sector_offset_in_run, copy_len);
                            
                            run_output_ptr += copy_len;
                            bytes_remaining_in_run -= copy_len;
                            current_sector++;
                            sector_offset_in_run = 0; // Next sectors start at 0
                        }
                        kfree(sector_temp);
                        
                        output_ptr += read_size;
                        bytes_read_so_far += read_size;
                    }
                }
                
                *bytes_read = to_read;
                kfree(mft_buffer);
                return ERR_OK;
            }
        }
        
        // Move to next attribute
        attr_offset += attr_length;
        attr_ptr += attr_length;
    }
    
    kfree(mft_buffer);
    *bytes_read = 0;
    return ERR_NOT_FOUND;
}