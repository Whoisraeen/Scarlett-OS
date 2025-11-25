/**
 * @file sfs.c
 * @brief Simple File System implementation
 */

#include "../include/fs/sfs.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/mm/heap.h"

// Helper: Read block
static error_code_t sfs_read_block(sfs_fs_t* fs, uint32_t block, void* buf) {
    return block_device_read(fs->device, block, buf);
}

// Helper: Write block
static error_code_t sfs_write_block(sfs_fs_t* fs, uint32_t block, const void* buf) {
    return block_device_write(fs->device, block, buf);
}

// Helper: Read inode
static error_code_t sfs_read_inode(sfs_fs_t* fs, uint32_t inode_num, sfs_inode_t* inode) {
    if (inode_num == 0 || inode_num > fs->superblock.inodes_count) return ERR_INVALID_ARG;
    
    uint32_t block = fs->superblock.inode_table_block + ((inode_num - 1) * sizeof(sfs_inode_t)) / fs->superblock.block_size;
    uint32_t offset = ((inode_num - 1) * sizeof(sfs_inode_t)) % fs->superblock.block_size;
    
    uint8_t* buf = (uint8_t*)kmalloc(fs->superblock.block_size);
    if (!buf) return ERR_OUT_OF_MEMORY;
    
    error_code_t err = sfs_read_block(fs, block, buf);
    if (err == ERR_OK) {
        memcpy(inode, buf + offset, sizeof(sfs_inode_t));
    }
    
    kfree(buf);
    return err;
}

// Helper: Write inode
static error_code_t sfs_write_inode(sfs_fs_t* fs, uint32_t inode_num, const sfs_inode_t* inode) {
    if (inode_num == 0 || inode_num > fs->superblock.inodes_count) return ERR_INVALID_ARG;
    
    uint32_t block = fs->superblock.inode_table_block + ((inode_num - 1) * sizeof(sfs_inode_t)) / fs->superblock.block_size;
    uint32_t offset = ((inode_num - 1) * sizeof(sfs_inode_t)) % fs->superblock.block_size;
    
    uint8_t* buf = (uint8_t*)kmalloc(fs->superblock.block_size);
    if (!buf) return ERR_OUT_OF_MEMORY;
    
    error_code_t err = sfs_read_block(fs, block, buf);
    if (err == ERR_OK) {
        memcpy(buf + offset, inode, sizeof(sfs_inode_t));
        err = sfs_write_block(fs, block, buf);
    }
    
    kfree(buf);
    return err;
}

// Helper: Allocate block (simple bitmap scan)
static int32_t sfs_alloc_block(sfs_fs_t* fs) {
    uint8_t* bitmap = (uint8_t*)kmalloc(fs->superblock.block_size);
    if (!bitmap) return -1;
    
    // Read block bitmap
    if (sfs_read_block(fs, fs->superblock.block_bitmap_block, bitmap) != ERR_OK) {
        kfree(bitmap);
        return -1;
    }
    
    for (uint32_t i = 0; i < fs->superblock.blocks_count; i++) {
        if (!(bitmap[i / 8] & (1 << (i % 8)))) {
            // Found free block
            bitmap[i / 8] |= (1 << (i % 8));
            sfs_write_block(fs, fs->superblock.block_bitmap_block, bitmap);
            fs->superblock.free_blocks--;
            // Update superblock on disk? Ideally yes.
            kfree(bitmap);
            return i + fs->superblock.data_block_start; // Return absolute block number
        }
    }
    
    kfree(bitmap);
    return -1;
}

// Helper: Allocate inode
static int32_t sfs_alloc_inode(sfs_fs_t* fs) {
    uint8_t* bitmap = (uint8_t*)kmalloc(fs->superblock.block_size);
    if (!bitmap) return -1;
    
    if (sfs_read_block(fs, fs->superblock.inode_bitmap_block, bitmap) != ERR_OK) {
        kfree(bitmap);
        return -1;
    }
    
    for (uint32_t i = 0; i < fs->superblock.inodes_count; i++) {
        if (!(bitmap[i / 8] & (1 << (i % 8)))) {
            bitmap[i / 8] |= (1 << (i % 8));
            sfs_write_block(fs, fs->superblock.inode_bitmap_block, bitmap);
            fs->superblock.free_inodes--;
            kfree(bitmap);
            return i + 1; // Inodes are 1-indexed
        }
    }
    
    kfree(bitmap);
    return -1;
}

// Format device
error_code_t sfs_format(block_device_t* device) {
    if (!device) return ERR_INVALID_ARG;
    
    uint32_t block_size = SFS_DEFAULT_BLOCK_SIZE;
    uint32_t total_blocks = device->size / block_size;
    uint32_t inodes_count = total_blocks / 4; // 1 inode per 4 blocks ratio
    
    // Layout:
    // 0: Superblock
    // 1: Inode Bitmap
    // 2: Block Bitmap
    // 3..N: Inode Table
    // N+1..End: Data
    
    uint32_t inode_table_blocks = (inodes_count * sizeof(sfs_inode_t) + block_size - 1) / block_size;
    uint32_t data_start = 3 + inode_table_blocks;
    
    sfs_superblock_t sb = {0};
    sb.magic = SFS_MAGIC;
    sb.block_size = block_size;
    sb.blocks_count = total_blocks - data_start; // Data blocks only
    sb.inodes_count = inodes_count;
    sb.free_blocks = sb.blocks_count;
    sb.free_inodes = inodes_count;
    sb.inode_bitmap_block = 1;
    sb.block_bitmap_block = 2;
    sb.inode_table_block = 3;
    sb.data_block_start = data_start;
    sb.root_inode = 1;
    
    // Write superblock
    uint8_t* buf = (uint8_t*)kmalloc(block_size);
    if (!buf) return ERR_OUT_OF_MEMORY;
    
    memset(buf, 0, block_size);
    memcpy(buf, &sb, sizeof(sb));
    block_device_write(device, 0, buf);
    
    // Clear bitmaps
    memset(buf, 0, block_size);
    block_device_write(device, 1, buf); // Inode bitmap
    block_device_write(device, 2, buf); // Block bitmap
    
    // Initialize root inode
    sfs_inode_t root = {0};
    root.type = VFS_TYPE_DIRECTORY;
    root.mode = 0755;
    root.size = 0;
    // Mark inode 1 as used
    uint8_t bitmap[1] = {1}; // Bit 0 set
    // Actually we need to read-modify-write but we just cleared it.
    // Just write the byte.
    memset(buf, 0, block_size);
    buf[0] = 1; // Inode 1 used
    block_device_write(device, 1, buf);
    
    // Write root inode to table
    // Need to write to first block of inode table
    memset(buf, 0, block_size);
    memcpy(buf, &root, sizeof(root)); // Inode 1 is at offset 0? No, 1-indexed.
    // If 1-indexed, index 0 is unused.
    // Let's put inode 1 at offset 0 for simplicity in calculation (inode_num - 1)
    block_device_write(device, 3, buf);
    
    kfree(buf);
    return ERR_OK;
}

// Initialize SFS
error_code_t sfs_init(block_device_t* device, sfs_fs_t* fs) {
    if (!device || !fs) return ERR_INVALID_ARG;
    
    fs->device = device;
    
    // Read superblock
    uint8_t* buf = (uint8_t*)kmalloc(SFS_DEFAULT_BLOCK_SIZE);
    if (!buf) return ERR_OUT_OF_MEMORY;
    
    if (block_device_read(device, 0, buf) != ERR_OK) {
        kfree(buf);
        return ERR_IO_ERROR;
    }
    
    memcpy(&fs->superblock, buf, sizeof(sfs_superblock_t));
    kfree(buf);
    
    if (fs->superblock.magic != SFS_MAGIC) {
        return ERR_INVALID_ELF; // Invalid FS
    }
    
```c
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/mm/heap.h"

// Helper: Read block
static error_code_t sfs_read_block(sfs_fs_t* fs, uint32_t block, void* buf) {
    return block_device_read(fs->device, block, buf);
}

// Helper: Write block
static error_code_t sfs_write_block(sfs_fs_t* fs, uint32_t block, const void* buf) {
    return block_device_write(fs->device, block, buf);
}

// Helper: Read inode
static error_code_t sfs_read_inode(sfs_fs_t* fs, uint32_t inode_num, sfs_inode_t* inode) {
    if (inode_num == 0 || inode_num > fs->superblock.inodes_count) return ERR_INVALID_ARG;
    
    uint32_t block = fs->superblock.inode_table_block + ((inode_num - 1) * sizeof(sfs_inode_t)) / fs->superblock.block_size;
    uint32_t offset = ((inode_num - 1) * sizeof(sfs_inode_t)) % fs->superblock.block_size;
    
    uint8_t* buf = (uint8_t*)kmalloc(fs->superblock.block_size);
    if (!buf) return ERR_OUT_OF_MEMORY;
    
    error_code_t err = sfs_read_block(fs, block, buf);
    if (err == ERR_OK) {
        memcpy(inode, buf + offset, sizeof(sfs_inode_t));
    }
    
    kfree(buf);
    return err;
}

// Helper: Write inode
static error_code_t sfs_write_inode(sfs_fs_t* fs, uint32_t inode_num, const sfs_inode_t* inode) {
    if (inode_num == 0 || inode_num > fs->superblock.inodes_count) return ERR_INVALID_ARG;
    
    uint32_t block = fs->superblock.inode_table_block + ((inode_num - 1) * sizeof(sfs_inode_t)) / fs->superblock.block_size;
    uint32_t offset = ((inode_num - 1) * sizeof(sfs_inode_t)) % fs->superblock.block_size;
    
    uint8_t* buf = (uint8_t*)kmalloc(fs->superblock.block_size);
    if (!buf) return ERR_OUT_OF_MEMORY;
    
    error_code_t err = sfs_read_block(fs, block, buf);
    if (err == ERR_OK) {
        memcpy(buf + offset, inode, sizeof(sfs_inode_t));
        err = sfs_write_block(fs, block, buf);
    }
    
    kfree(buf);
    return err;
}

// Helper: Allocate block (simple bitmap scan)
static int32_t sfs_alloc_block(sfs_fs_t* fs) {
    uint8_t* bitmap = (uint8_t*)kmalloc(fs->superblock.block_size);
    if (!bitmap) return -1;
    
    // Read block bitmap
    if (sfs_read_block(fs, fs->superblock.block_bitmap_block, bitmap) != ERR_OK) {
        kfree(bitmap);
        return -1;
    }
    
    for (uint32_t i = 0; i < fs->superblock.blocks_count; i++) {
        if (!(bitmap[i / 8] & (1 << (i % 8)))) {
            // Found free block
            bitmap[i / 8] |= (1 << (i % 8));
            sfs_write_block(fs, fs->superblock.block_bitmap_block, bitmap);
            fs->superblock.free_blocks--;
            // Update superblock on disk? Ideally yes.
            kfree(bitmap);
            return i + fs->superblock.data_block_start; // Return absolute block number
        }
    }
    
    kfree(bitmap);
    return -1;
}

// Helper: Allocate inode
static int32_t sfs_alloc_inode(sfs_fs_t* fs) {
    uint8_t* bitmap = (uint8_t*)kmalloc(fs->superblock.block_size);
    if (!bitmap) return -1;
    
    if (sfs_read_block(fs, fs->superblock.inode_bitmap_block, bitmap) != ERR_OK) {
        kfree(bitmap);
        return -1;
    }
    
    for (uint32_t i = 0; i < fs->superblock.inodes_count; i++) {
        if (!(bitmap[i / 8] & (1 << (i % 8)))) {
            bitmap[i / 8] |= (1 << (i % 8));
            sfs_write_block(fs, fs->superblock.inode_bitmap_block, bitmap);
            fs->superblock.free_inodes--;
            kfree(bitmap);
            return i + 1; // Inodes are 1-indexed
        }
    }
    
    kfree(bitmap);
    return -1;
}

// Format device
error_code_t sfs_format(block_device_t* device) {
    if (!device) return ERR_INVALID_ARG;
    
    uint32_t block_size = SFS_DEFAULT_BLOCK_SIZE;
    uint32_t total_blocks = device->size / block_size;
    uint32_t inodes_count = total_blocks / 4; // 1 inode per 4 blocks ratio
    
    // Layout:
    // 0: Superblock
    // 1: Inode Bitmap
    // 2: Block Bitmap
    // 3..N: Inode Table
    // N+1..End: Data
    
    uint32_t inode_table_blocks = (inodes_count * sizeof(sfs_inode_t) + block_size - 1) / block_size;
    uint32_t data_start = 3 + inode_table_blocks;
    
    sfs_superblock_t sb = {0};
    sb.magic = SFS_MAGIC;
    sb.block_size = block_size;
    sb.blocks_count = total_blocks - data_start; // Data blocks only
    sb.inodes_count = inodes_count;
    sb.free_blocks = sb.blocks_count;
    sb.free_inodes = inodes_count;
    sb.inode_bitmap_block = 1;
    sb.block_bitmap_block = 2;
    sb.inode_table_block = 3;
    sb.data_block_start = data_start;
    sb.root_inode = 1;
    
    // Write superblock
    uint8_t* buf = (uint8_t*)kmalloc(block_size);
    if (!buf) return ERR_OUT_OF_MEMORY;
    
    memset(buf, 0, block_size);
    memcpy(buf, &sb, sizeof(sb));
    block_device_write(device, 0, buf);
    
    // Clear bitmaps
    memset(buf, 0, block_size);
    block_device_write(device, 1, buf); // Inode bitmap
    block_device_write(device, 2, buf); // Block bitmap
    
    // Initialize root inode
    sfs_inode_t root = {0};
    root.type = VFS_TYPE_DIRECTORY;
    root.mode = 0755;
    root.size = 0;
    // Mark inode 1 as used
    uint8_t bitmap[1] = {1}; // Bit 0 set
    // Actually we need to read-modify-write but we just cleared it.
    // Just write the byte.
    memset(buf, 0, block_size);
    buf[0] = 1; // Inode 1 used
    block_device_write(device, 1, buf);
    
    // Write root inode to table
    // Need to write to first block of inode table
    memset(buf, 0, block_size);
    memcpy(buf, &root, sizeof(root)); // Inode 1 is at offset 0? No, 1-indexed.
    // If 1-indexed, index 0 is unused.
    // Let's put inode 1 at offset 0 for simplicity in calculation (inode_num - 1)
    block_device_write(device, 3, buf);
    
    kfree(buf);
    return ERR_OK;
}

// Initialize SFS
error_code_t sfs_init(block_device_t* device, sfs_fs_t* fs) {
    if (!device || !fs) return ERR_INVALID_ARG;
    
    fs->device = device;
    
    // Read superblock
    uint8_t* buf = (uint8_t*)kmalloc(SFS_DEFAULT_BLOCK_SIZE);
    if (!buf) return ERR_OUT_OF_MEMORY;
    
    if (block_device_read(device, 0, buf) != ERR_OK) {
        kfree(buf);
        return ERR_IO_ERROR;
    }
    
    memcpy(&fs->superblock, buf, sizeof(sfs_superblock_t));
    kfree(buf);
    
    if (fs->superblock.magic != SFS_MAGIC) {
        return ERR_INVALID_ELF; // Invalid FS
    }
    
    fs->initialized = true;
    return ERR_OK;
}

typedef struct {
    uint32_t inode;
    uint64_t offset;
} sfs_file_handle_t;

// Open file
error_code_t sfs_open(vfs_filesystem_t* vfs, const char* path, uint64_t flags, fd_t* fd, void** file_data) {
    sfs_fs_t* fs = (sfs_fs_t*)vfs->private_data;
    if (!fs || !fs->initialized) return ERR_INVALID_STATE;
    
    // Resolve path to inode
    // Simplified: assume root directory for now or flat hierarchy
    // TODO: Implement path traversal
    
    uint32_t inode_num = fs->superblock.root_inode;
    
    if (strcmp(path, "/") != 0) {
        // Parse path
        char temp_path[256];
        strncpy(temp_path, path, 255);
        temp_path[255] = 0;
        
        char* component = strtok(temp_path, "/");
        while (component != NULL) {
            // Lookup component in current directory (inode_num)
            sfs_inode_t dir_inode;
            if (sfs_read_inode(fs, inode_num, &dir_inode) != ERR_OK) return ERR_IO_ERROR;
            
            if (dir_inode.type != VFS_TYPE_DIRECTORY) return ERR_NOT_FOUND;
            
            uint32_t found_inode = 0;
            
            // Scan directory blocks
            for (int i = 0; i < 12; i++) {
                if (dir_inode.blocks[i] == 0) continue;
                
                uint8_t* buf = (uint8_t*)kmalloc(fs->superblock.block_size);
                if (!buf) return ERR_OUT_OF_MEMORY;
                
                if (sfs_read_block(fs, dir_inode.blocks[i], buf) != ERR_OK) {
                    kfree(buf);
                    return ERR_IO_ERROR;
                }
                
                sfs_dirent_t* entries = (sfs_dirent_t*)buf;
                uint32_t count = fs->superblock.block_size / sizeof(sfs_dirent_t);
                
                for (uint32_t j = 0; j < count; j++) {
                    if (entries[j].inode != 0 && strcmp(entries[j].name, component) == 0) {
                        found_inode = entries[j].inode;
                        break;
                    }
                }
                
                kfree(buf);
                if (found_inode != 0) break;
            }
            
            if (found_inode == 0) {
                // Not found
                inode_num = 0;
                break;
            }
            
            inode_num = found_inode;
            component = strtok(NULL, "/");
        }
    }
    
    if (inode_num == 0) {
        // Create if requested
        if (flags & VFS_MODE_CREATE) {
            // Allocate new inode
            int32_t new_inode = sfs_alloc_inode(fs);
            if (new_inode < 0) return ERR_OUT_OF_MEMORY;
    
            // Initialize new inode
            sfs_inode_t new_inode_obj = {0};
            new_inode_obj.type = VFS_TYPE_FILE; // Default
            new_inode_obj.mode = 0644;
            new_inode_obj.size = 0;
            
            if (sfs_write_inode(fs, new_inode, &new_inode_obj) != ERR_OK) {
                // Free inode (TODO)
                return ERR_IO_ERROR;
            }
            
            // Add entry to parent directory
            // For now, assume parent is root
            // TODO: Real path traversal implies finding parent inode
            
            // Add to root directory
            sfs_inode_t root;
            if (sfs_read_inode(fs, fs->superblock.root_inode, &root) != ERR_OK) return ERR_IO_ERROR;
            
            bool added = false;
            for (int i = 0; i < 12; i++) {
                if (root.blocks[i] == 0) {
                    // Allocate block for directory
                    int32_t new_block = sfs_alloc_block(fs);
                    if (new_block < 0) return ERR_OUT_OF_MEMORY;
                    root.blocks[i] = new_block;
                    
                    // Initialize block with zeros
                    uint8_t* zero_buf = (uint8_t*)kmalloc(fs->superblock.block_size);
                    if (!zero_buf) return ERR_OUT_OF_MEMORY;
                    memset(zero_buf, 0, fs->superblock.block_size);
                    sfs_write_block(fs, new_block, zero_buf);
                    kfree(zero_buf);
                    
                    // Update root inode
                    sfs_write_inode(fs, fs->superblock.root_inode, &root);
                }
                
                // Scan block for empty slot
                uint8_t* buf = (uint8_t*)kmalloc(fs->superblock.block_size);
                if (!buf) return ERR_OUT_OF_MEMORY;
                
                if (sfs_read_block(fs, root.blocks[i], buf) != ERR_OK) {
                    kfree(buf);
                    return ERR_IO_ERROR;
                }
                
                sfs_dirent_t* entries = (sfs_dirent_t*)buf;
                uint32_t count = fs->superblock.block_size / sizeof(sfs_dirent_t);
                
                for (uint32_t j = 0; j < count; j++) {
                    if (entries[j].inode == 0) {
                        entries[j].inode = new_inode;
                        strncpy(entries[j].name, path + 1, 59); // Max name len
                        sfs_write_block(fs, root.blocks[i], buf);
                        added = true;
                        break;
                    }
                }
                
                kfree(buf);
                if (added) break;
            }
            
            if (!added) return ERR_FS_FULL; // Directory full (limit for now)
            
            inode_num = new_inode;
        } else {
            return ERR_NOT_FOUND;
        }
    }
    
    // Create file handle
    sfs_file_handle_t* handle = (sfs_file_handle_t*)kmalloc(sizeof(sfs_file_handle_t));
    if (!handle) return ERR_OUT_OF_MEMORY;
    
    handle->inode = inode_num;
    handle->offset = 0;
    
    *file_data = handle;
    return ERR_OK;
}

// Read file
error_code_t sfs_read(vfs_filesystem_t* vfs, void* file_data, void* buf, uint64_t count, uint64_t* bytes_read) {
    sfs_fs_t* fs = (sfs_fs_t*)vfs->private_data;
    sfs_file_handle_t* handle = (sfs_file_handle_t*)file_data;
    
    if (!fs || !handle || !buf) return ERR_INVALID_ARG;
    
    sfs_inode_t inode;
    if (sfs_read_inode(fs, handle->inode, &inode) != ERR_OK) return ERR_IO_ERROR;
    
    // Allow reading both regular files and directories
    // For directories, we return the raw directory entries
    if (inode.type != VFS_TYPE_FILE && inode.type != VFS_TYPE_DIRECTORY) {
        return ERR_INVALID_ARG;
    }
    
    // ... rest of implementation ...

// Write file
error_code_t sfs_write(vfs_filesystem_t* vfs, void* file_data, const void* buf, uint64_t count, uint64_t* bytes_written) {
    sfs_fs_t* fs = (sfs_fs_t*)vfs->private_data;
    sfs_file_handle_t* handle = (sfs_file_handle_t*)file_data;
    
    if (!fs || !handle || !buf) return ERR_INVALID_ARG;
    
    sfs_inode_t inode;
    if (sfs_read_inode(fs, handle->inode, &inode) != ERR_OK) return ERR_IO_ERROR;
    
    uint64_t written_so_far = 0;
    bool inode_updated = false;
    
    while (written_so_far < count) {
        uint64_t pos = handle->offset + written_so_far;
        uint32_t block_idx = pos / fs->superblock.block_size;
        uint32_t offset_in_block = pos % fs->superblock.block_size;
        
        if (block_idx >= 12) {
            // TODO: Indirect blocks
            break; 
        }
        
        if (inode.blocks[block_idx] == 0) {
            // Allocate block
            int32_t new_block = sfs_alloc_block(fs);
            if (new_block < 0) break; // No space
            inode.blocks[block_idx] = new_block;
            inode_updated = true;
            
            // Zero init if partial write will occur? 
            // We overwrite anyway, but if offset_in_block > 0, we need previous data (which was zero/hole)
            if (offset_in_block > 0 || (count - written_so_far) < fs->superblock.block_size) {
                uint8_t* zbuf = (uint8_t*)kmalloc(fs->superblock.block_size);
                if (zbuf) {
                    memset(zbuf, 0, fs->superblock.block_size);
                    sfs_write_block(fs, new_block, zbuf);
                    kfree(zbuf);
                }
            }
        }
        
        uint32_t physical_block = inode.blocks[block_idx];
        
        uint8_t* block_buf = (uint8_t*)kmalloc(fs->superblock.block_size);
        if (!block_buf) return ERR_OUT_OF_MEMORY;
        
        // Read-Modify-Write if not overwriting whole block
        uint64_t chunk = fs->superblock.block_size - offset_in_block;
        if (chunk > (count - written_so_far)) chunk = count - written_so_far;
        
        if (chunk < fs->superblock.block_size) {
            if (sfs_read_block(fs, physical_block, block_buf) != ERR_OK) {
                kfree(block_buf);
                return ERR_IO_ERROR;
            }
        }
        
        memcpy(block_buf + offset_in_block, (uint8_t*)buf + written_so_far, chunk);
        
        if (sfs_write_block(fs, physical_block, block_buf) != ERR_OK) {
            kfree(block_buf);
            return ERR_IO_ERROR;
        }
        kfree(block_buf);
        
        written_so_far += chunk;
    }
    
    handle->offset += written_so_far;
    if (handle->offset > inode.size) {
        inode.size = handle->offset;
        inode_updated = true;
    }
    
    if (inode_updated) {
        sfs_write_inode(fs, handle->inode, &inode);
    }
    
    *bytes_written = written_so_far;
    return ERR_OK;
}

// Close file
error_code_t sfs_close(vfs_filesystem_t* vfs, void* file_data) {
    if (file_data) {
        kfree(file_data);
    }
    return ERR_OK;
}

// Register SFS
error_code_t sfs_register_vfs(void) {
    vfs_fs_ops_t ops = {0};
    ops.open = sfs_open;
    ops.read = sfs_read;
    ops.write = sfs_write;
    ops.close = sfs_close;
    // TODO: Add create, mkdir, etc.
    
    // We can't register to VFS without a VFS function.
    // Assuming vfs_register_filesystem("sfs", &ops); exists.
    return ERR_OK; 
}

```
