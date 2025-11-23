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

/**
 * Find file in NTFS (simplified - would need full path parsing)
 */
error_code_t ntfs_find_file(ntfs_fs_t* fs, const char* path, uint64_t* mft_record) {
    if (!fs || !fs->initialized || !path || !mft_record) {
        return ERR_INVALID_ARG;
    }
    
    // Implement full path resolution
    // Parse path components
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
    
    // Start from root directory (MFT record 5)
    uint64_t current_record = 5;
    
    // Traverse path
    for (uint32_t i = 0; i < component_count; i++) {
        // Read current MFT record
        uint8_t* mft_buffer = (uint8_t*)kmalloc(fs->mft_record_size);
        if (!mft_buffer) {
            return ERR_OUT_OF_MEMORY;
        }
        
        error_code_t err = ntfs_read_mft_record(fs, current_record, mft_buffer);
        if (err != ERR_OK) {
            kfree(mft_buffer);
            return err;
        }
        
        // Find $FILE_NAME attribute and search directory
        // Simplified: would need full attribute parsing
        // For now, return error indicating full implementation needed
        kfree(mft_buffer);
        
        // TODO: Parse $FILE_NAME attributes, search for component name
        // This requires full attribute parsing which is complex
        return ERR_NOT_SUPPORTED;
    }
    
    *mft_record = current_record;
    return ERR_OK;
}

/**
 * Read file data from NTFS (simplified)
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
                        run_offset |= ((int64_t)(int8_t)*run_ptr++) << (i * 8);
                    }
                    
                    current_lcn += run_offset;
                    current_vcn += run_length;
                    
                    // Calculate if this run contains data we need
                    uint64_t run_start_byte = current_vcn * fs->bytes_per_cluster;
                    uint64_t run_end_byte = (current_vcn + run_length) * fs->bytes_per_cluster;
                    
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
                        uint8_t* read_buffer = (uint8_t*)kmalloc(read_size);
                        if (!read_buffer) {
                            kfree(mft_buffer);
                            return ERR_OUT_OF_MEMORY;
                        }
                        
                        uint32_t sectors_to_read = (read_size + fs->bytes_per_sector - 1) / fs->bytes_per_sector;
                        for (uint32_t s = 0; s < sectors_to_read; s++) {
                            error_code_t err = block_device_read(fs->device, sector + s, read_buffer + (s * fs->bytes_per_sector));
                            if (err != ERR_OK) {
                                kfree(read_buffer);
                                kfree(mft_buffer);
                                return err;
                            }
                        }
                        
                        memcpy(output_ptr, read_buffer + (read_start - run_start_byte - (cluster_offset * fs->bytes_per_cluster)), read_size);
                        output_ptr += read_size;
                        bytes_read_so_far += read_size;
                        
                        kfree(read_buffer);
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

