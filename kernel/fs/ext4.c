/**
 * @file ext4.c
 * @brief ext4 filesystem implementation
 */

#include "../include/fs/ext4.h"
#include "../include/fs/block.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/mm/heap.h"

/**
 * Read ext4 superblock
 */
static error_code_t ext4_read_superblock(block_device_t* device, ext4_superblock_t* superblock) {
    if (!device || !superblock) {
        return ERR_INVALID_ARG;
    }
    
    // ext4 superblock is at block 1 (block 0 is boot sector)
    error_code_t err = block_device_read(device, 1, superblock);
    if (err != ERR_OK) {
        return err;
    }
    
    // Verify magic number
    if (superblock->magic != EXT4_SUPER_MAGIC) {
        kerror("ext4: Invalid magic number (0x%04x)\n", superblock->magic);
        return ERR_INVALID_ELF;  // Reuse error code
    }
    
    return ERR_OK;
}

/**
 * Initialize ext4 filesystem
 */
error_code_t ext4_init(block_device_t* device, ext4_fs_t* fs) {
    if (!device || !fs) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("Initializing ext4 filesystem on device %s...\n", device->name);
    
    // Read superblock
    error_code_t err = ext4_read_superblock(device, &fs->superblock);
    if (err != ERR_OK) {
        return err;
    }
    
    // Calculate filesystem parameters
    fs->device = device;
    fs->block_size = 1024 << fs->superblock.log_block_size;
    fs->inode_size = fs->superblock.inode_size ? fs->superblock.inode_size : 128;
    fs->blocks_per_group = fs->superblock.blocks_per_group;
    fs->inodes_per_group = fs->superblock.inodes_per_group;
    
    // Calculate number of block groups
    fs->group_count = (fs->superblock.blocks_count + fs->blocks_per_group - 1) / fs->blocks_per_group;
    
    fs->initialized = true;
    
    kinfo("ext4: Block size: %u, Inode size: %u, Groups: %u\n",
          fs->block_size, fs->inode_size, fs->group_count);
    kinfo("ext4: Total blocks: %u, Free blocks: %u\n",
          fs->superblock.blocks_count, fs->superblock.free_blocks_count);
    kinfo("ext4: Total inodes: %u, Free inodes: %u\n",
          fs->superblock.inodes_count, fs->superblock.free_inodes_count);
    
    return ERR_OK;
}

/**
 * Mount ext4 filesystem
 */
error_code_t ext4_mount(ext4_fs_t* fs, const char* mountpoint) {
    if (!fs || !fs->initialized) {
        return ERR_INVALID_STATE;
    }
    
    (void)mountpoint;  // For now, just verify filesystem is valid
    kinfo("ext4: Mounted at %s\n", mountpoint);
    return ERR_OK;
}

/**
 * Unmount ext4 filesystem
 */
error_code_t ext4_unmount(ext4_fs_t* fs) {
    if (!fs) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("ext4: Unmounting...\n");
    fs->initialized = false;
    return ERR_OK;
}

/**
 * Calculate block group for inode
 */
static uint32_t ext4_inode_to_group(ext4_fs_t* fs, uint32_t inode_num) {
    return (inode_num - 1) / fs->inodes_per_group;
}

/**
 * Calculate inode index within group
 */
static uint32_t ext4_inode_to_index(ext4_fs_t* fs, uint32_t inode_num) {
    return (inode_num - 1) % fs->inodes_per_group;
}

/**
 * Read inode from filesystem
 */
error_code_t ext4_read_inode(ext4_fs_t* fs, uint32_t inode_num, ext4_inode_t* inode) {
    if (!fs || !fs->initialized || !inode) {
        return ERR_INVALID_ARG;
    }
    
    if (inode_num == 0 || inode_num > fs->superblock.inodes_count) {
        return ERR_NOT_FOUND;
    }
    
    // Calculate block group and inode index
    uint32_t group = ext4_inode_to_group(fs, inode_num);
    uint32_t index = ext4_inode_to_index(fs, inode_num);
    
    // Read group descriptor to find inode table block
    // Group descriptors start at block (first_data_block + 1) or block 1 if block size >= 2KB
    uint32_t first_data_block = fs->superblock.first_data_block;
    uint32_t group_desc_block = (fs->block_size >= 2048) ? 1 : (first_data_block + 1);
    
    // Calculate which group descriptor to read
    // Group descriptors are typically 32 bytes each
    size_t group_desc_size = fs->superblock.group_desc_size ? fs->superblock.group_desc_size : 32;
    uint32_t descs_per_block = fs->block_size / group_desc_size;
    uint32_t desc_block = group_desc_block + (group / descs_per_block);
    uint32_t desc_offset = (group % descs_per_block) * group_desc_size;
    
    // Read group descriptor block
    uint8_t* desc_block_buffer = (uint8_t*)kmalloc(fs->block_size);
    if (!desc_block_buffer) {
        return ERR_OUT_OF_MEMORY;
    }
    
    error_code_t err = block_device_read(fs->device, desc_block, desc_block_buffer);
    if (err != ERR_OK) {
        kfree(desc_block_buffer);
        return err;
    }
    
    // Extract inode table block from group descriptor
    // Group descriptor structure: {block_bitmap, inode_bitmap, inode_table, ...}
    // inode_table is at offset 8 (32-bit value)
    uint32_t* desc = (uint32_t*)(desc_block_buffer + desc_offset);
    uint32_t inode_table_block = desc[2];  // inode_table is 3rd 32-bit field (index 2)
    
    kfree(desc_block_buffer);
    
    // Calculate block and offset within block
    uint32_t inode_block = inode_table_block + (index * fs->inode_size) / fs->block_size;
    uint32_t inode_offset = (index * fs->inode_size) % fs->block_size;
    
    // Read block containing inode
    uint8_t* block_buffer = (uint8_t*)kmalloc(fs->block_size);
    if (!block_buffer) {
        return ERR_OUT_OF_MEMORY;
    }
    
    err = block_device_read(fs->device, inode_block, block_buffer);
    if (err != ERR_OK) {
        kfree(block_buffer);
        return err;
    }

    // Copy inode from block
    memcpy(inode, block_buffer + inode_offset, sizeof(ext4_inode_t));
    
    kfree(block_buffer);
    return ERR_OK;
}

/**
 * Read block from inode (handles direct and indirect blocks)
 */
static error_code_t ext4_read_inode_block(ext4_fs_t* fs, ext4_inode_t* inode, uint32_t block_index, uint8_t* buffer) {
    if (!fs || !inode || !buffer) {
        return ERR_INVALID_ARG;
    }
    
    uint32_t block_num = 0;
    
    // Direct blocks (0-11)
    if (block_index < 12) {
        block_num = inode->block[block_index];
    }
    // Indirect block (12)
    else if (block_index < 12 + (fs->block_size / 4)) {
        // Single indirect: block[12] points to a block containing block numbers
        uint32_t indirect_index = block_index - 12;
        uint32_t indirect_block = inode->block[12];
        
        if (indirect_block == 0) {
            return ERR_NOT_FOUND;
        }
        
        // Read indirect block
        uint8_t* indirect_buffer = (uint8_t*)kmalloc(fs->block_size);
        if (!indirect_buffer) {
            return ERR_OUT_OF_MEMORY;
        }
        
        error_code_t err = block_device_read(fs->device, indirect_block, indirect_buffer);
        if (err != ERR_OK) {
            kfree(indirect_buffer);
            return err;
        }
        
        // Get block number from indirect block
        uint32_t* block_ptr = (uint32_t*)indirect_buffer;
        block_num = block_ptr[indirect_index];
        
        kfree(indirect_buffer);
    }
    // Double indirect (13)
    else if (block_index < 12 + (fs->block_size / 4) + (fs->block_size / 4) * (fs->block_size / 4)) {
        // Double indirect: block[13] points to a block of indirect block pointers
        uint32_t blocks_per_indirect = fs->block_size / 4;
        uint32_t double_index = block_index - 12 - blocks_per_indirect;
        uint32_t indirect1_index = double_index / blocks_per_indirect;
        uint32_t indirect2_index = double_index % blocks_per_indirect;
        uint32_t double_indirect_block = inode->block[13];
        
        if (double_indirect_block == 0) {
            return ERR_NOT_FOUND;
        }
        
        // Read double indirect block
        uint8_t* double_indirect_buffer = (uint8_t*)kmalloc(fs->block_size);
        if (!double_indirect_buffer) {
            return ERR_OUT_OF_MEMORY;
        }
        
        error_code_t err = block_device_read(fs->device, double_indirect_block, double_indirect_buffer);
        if (err != ERR_OK) {
            kfree(double_indirect_buffer);
            return err;
        }
        
        // Get indirect block number
        uint32_t* double_block_ptr = (uint32_t*)double_indirect_buffer;
        uint32_t indirect_block = double_block_ptr[indirect1_index];
        kfree(double_indirect_buffer);
        
        if (indirect_block == 0) {
            return ERR_NOT_FOUND;
        }
        
        // Read indirect block
        uint8_t* indirect_buffer = (uint8_t*)kmalloc(fs->block_size);
        if (!indirect_buffer) {
            return ERR_OUT_OF_MEMORY;
        }
        
        err = block_device_read(fs->device, indirect_block, indirect_buffer);
        if (err != ERR_OK) {
            kfree(indirect_buffer);
            return err;
        }
        
        // Get block number from indirect block
        uint32_t* block_ptr = (uint32_t*)indirect_buffer;
        block_num = block_ptr[indirect2_index];
        
        kfree(indirect_buffer);
    }
    // Triple indirect (14)
    else {
        // Triple indirect: block[14] points to a block of double indirect block pointers
        uint32_t blocks_per_indirect = fs->block_size / 4;
        uint32_t triple_index = block_index - 12 - blocks_per_indirect - (blocks_per_indirect * blocks_per_indirect);
        uint32_t double_index = triple_index / blocks_per_indirect;
        uint32_t indirect1_index = (triple_index % blocks_per_indirect) / blocks_per_indirect;
        uint32_t indirect2_index = triple_index % blocks_per_indirect;
        uint32_t triple_indirect_block = inode->block[14];
        
        if (triple_indirect_block == 0) {
            return ERR_NOT_FOUND;
        }
        
        // Read triple indirect block
        uint8_t* triple_indirect_buffer = (uint8_t*)kmalloc(fs->block_size);
        if (!triple_indirect_buffer) {
            return ERR_OUT_OF_MEMORY;
        }
        
        error_code_t err = block_device_read(fs->device, triple_indirect_block, triple_indirect_buffer);
        if (err != ERR_OK) {
            kfree(triple_indirect_buffer);
            return err;
        }
        
        // Get double indirect block number
        uint32_t* triple_block_ptr = (uint32_t*)triple_indirect_buffer;
        uint32_t double_indirect_block = triple_block_ptr[double_index];
        kfree(triple_indirect_buffer);
        
        if (double_indirect_block == 0) {
            return ERR_NOT_FOUND;
        }
        
        // Read double indirect block
        uint8_t* double_indirect_buffer = (uint8_t*)kmalloc(fs->block_size);
        if (!double_indirect_buffer) {
            return ERR_OUT_OF_MEMORY;
        }
        
        err = block_device_read(fs->device, double_indirect_block, double_indirect_buffer);
        if (err != ERR_OK) {
            kfree(double_indirect_buffer);
            return err;
        }
        
        // Get indirect block number
        uint32_t* double_block_ptr = (uint32_t*)double_indirect_buffer;
        uint32_t indirect_block = double_block_ptr[indirect1_index];
        kfree(double_indirect_buffer);
        
        if (indirect_block == 0) {
            return ERR_NOT_FOUND;
        }
        
        // Read indirect block
        uint8_t* indirect_buffer = (uint8_t*)kmalloc(fs->block_size);
        if (!indirect_buffer) {
            return ERR_OUT_OF_MEMORY;
        }
        
        err = block_device_read(fs->device, indirect_block, indirect_buffer);
        if (err != ERR_OK) {
            kfree(indirect_buffer);
            return err;
        }
        
        // Get block number from indirect block
        uint32_t* block_ptr = (uint32_t*)indirect_buffer;
        block_num = block_ptr[indirect2_index];
        
        kfree(indirect_buffer);
    }
    
    if (block_num == 0) {
        return ERR_NOT_FOUND;
    }
    
    return block_device_read(fs->device, block_num, buffer);
}

/**
 * Find file in directory
 */
error_code_t ext4_find_file(ext4_fs_t* fs, uint32_t parent_inode, const char* name, uint32_t* inode_num) {
    if (!fs || !fs->initialized || !name || !inode_num) {
        return ERR_INVALID_ARG;
    }
    
    // Read parent inode
    ext4_inode_t parent;
    error_code_t err = ext4_read_inode(fs, parent_inode, &parent);
    if (err != ERR_OK) {
        return err;
    }
    
    // Check if it's a directory
    if ((parent.mode & 0xF000) != 0x4000) {  // Directory type
        return ERR_NOT_DIRECTORY;
    }
    
    // Read directory blocks
    uint8_t* block_buffer = (uint8_t*)kmalloc(fs->block_size);
    if (!block_buffer) {
        return ERR_OUT_OF_MEMORY;
    }
    
    size_t file_size = parent.size_lo | ((uint64_t)parent.size_hi << 32);
    size_t blocks_to_read = (file_size + fs->block_size - 1) / fs->block_size;
    
    for (size_t i = 0; i < blocks_to_read && i < 12; i++) {  // Only check direct blocks for now
        err = ext4_read_inode_block(fs, &parent, i, block_buffer);
        if (err != ERR_OK) {
            kfree(block_buffer);
            return err;
        }
        
        // Parse directory entries
        size_t pos = 0;
        while (pos < fs->block_size) {
            ext4_dir_entry_t* entry = (ext4_dir_entry_t*)(block_buffer + pos);
            
            if (entry->inode == 0) {
                break;  // End of directory
            }
            
            if (entry->name_len == strlen(name) &&
                memcmp(entry->name, name, entry->name_len) == 0) {
                *inode_num = entry->inode;
                kfree(block_buffer);
                return ERR_OK;
            }
            
            pos += entry->rec_len;
            if (entry->rec_len == 0) {
                break;
            }
        }
    }
    
    kfree(block_buffer);
    return ERR_NOT_FOUND;
}

/**
 * Read file data
 */
error_code_t ext4_read_file(ext4_fs_t* fs, uint32_t inode_num, void* buffer, size_t offset, size_t count, size_t* bytes_read) {
    if (!fs || !fs->initialized || !buffer || !bytes_read) {
        return ERR_INVALID_ARG;
    }
    
    // Read inode
    ext4_inode_t inode;
    error_code_t err = ext4_read_inode(fs, inode_num, &inode);
    if (err != ERR_OK) {
        return err;
    }
    
    size_t file_size = inode.size_lo | ((uint64_t)inode.size_hi << 32);
    if (offset >= file_size) {
        *bytes_read = 0;
        return ERR_OK;
    }
    
    size_t to_read = count;
    if (offset + to_read > file_size) {
        to_read = file_size - offset;
    }
    
    // Calculate starting block
    uint32_t start_block = offset / fs->block_size;
    uint32_t start_offset = offset % fs->block_size;
    
    uint8_t* block_buffer = (uint8_t*)kmalloc(fs->block_size);
    if (!block_buffer) {
        return ERR_OUT_OF_MEMORY;
    }
    
    size_t bytes_copied = 0;
    uint32_t current_block = start_block;
    
    while (bytes_copied < to_read) {
        err = ext4_read_inode_block(fs, &inode, current_block, block_buffer);
        if (err != ERR_OK) {
            kfree(block_buffer);
            return err;
        }
        
        size_t copy_from_block = (current_block == start_block) ? start_offset : 0;
        size_t copy_len = fs->block_size - copy_from_block;
        if (bytes_copied + copy_len > to_read) {
            copy_len = to_read - bytes_copied;
        }
        
        memcpy((uint8_t*)buffer + bytes_copied, block_buffer + copy_from_block, copy_len);
        bytes_copied += copy_len;
        current_block++;
    }
    
    kfree(block_buffer);
    *bytes_read = bytes_copied;
    return ERR_OK;
}

/**
 * Read directory entries
 */
error_code_t ext4_read_dir(ext4_fs_t* fs, uint32_t inode_num, ext4_dir_entry_t* entries, size_t max_entries, size_t* entry_count) {
    if (!fs || !fs->initialized || !entries || !entry_count) {
        return ERR_INVALID_ARG;
    }
    
    // Read inode
    ext4_inode_t inode;
    error_code_t err = ext4_read_inode(fs, inode_num, &inode);
    if (err != ERR_OK) {
        return err;
    }
    
    // Check if it's a directory
    if ((inode.mode & 0xF000) != 0x4000) {
        return ERR_NOT_DIRECTORY;
    }
    
    size_t file_size = inode.size_lo | ((uint64_t)inode.size_hi << 32);
    size_t blocks_to_read = (file_size + fs->block_size - 1) / fs->block_size;
    
    uint8_t* block_buffer = (uint8_t*)kmalloc(fs->block_size);
    if (!block_buffer) {
        return ERR_OUT_OF_MEMORY;
    }
    
    *entry_count = 0;
    
    for (size_t i = 0; i < blocks_to_read && i < 12 && *entry_count < max_entries; i++) {
        err = ext4_read_inode_block(fs, &inode, i, block_buffer);
        if (err != ERR_OK) {
            kfree(block_buffer);
            return err;
        }
        
        // Parse directory entries
        size_t pos = 0;
        while (pos < fs->block_size && *entry_count < max_entries) {
            ext4_dir_entry_t* entry = (ext4_dir_entry_t*)(block_buffer + pos);
            
            if (entry->inode == 0) {
                break;  // End of directory
            }
            
            if (entry->rec_len == 0) {
                break;
            }
            
            // Copy entry (simplified - would need to handle variable-length names)
            if (*entry_count < max_entries) {
                memcpy(&entries[*entry_count], entry, sizeof(ext4_dir_entry_t));
                (*entry_count)++;
            }
            
            pos += entry->rec_len;
        }
    }
    
    kfree(block_buffer);
    return ERR_OK;
}

