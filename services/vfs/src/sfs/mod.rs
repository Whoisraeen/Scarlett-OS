//! Scarlett File System (SFS)
//!
//! A next-generation Copy-on-Write (CoW) file system with instant rollback,
//! native deduplication, compression, and per-app sandboxes.

pub mod superblock;
pub mod inode;
pub mod btree;
pub mod cow;
pub mod snapshot;
pub mod cache;

extern crate alloc;
use alloc::vec::Vec;
use alloc::string::String;
use core::convert::TryInto;

use crate::file_ops::*;
use superblock::*;
use inode::*;
use cow::*;
use snapshot::*;

// Syscall constants (copied from ipc.rs for convenience)
const SYS_IPC_SEND: u64 = 9;
const SYS_IPC_RECEIVE: u64 = 10;
const SYS_GET_UPTIME_MS: u64 = 47;

// Device Manager IPC constants
const DRIVER_MANAGER_PORT: u32 = 100; // From services/driver_manager/src/main.rs
const DM_MSG_OPEN_DEVICE: u32 = 6;    // Arbitrary new message ID for opening device

// IPC message structure (must match kernel/include/ipc/ipc.h)
#[repr(C)]
#[derive(Clone, Copy)]
struct IpcMessage {
    sender_tid: u64,
    msg_id: u64,
    msg_type: u32,
    inline_size: u32,
    inline_data: [u8; 64],
    buffer: u64,
    buffer_size: u64,
}

// Syscall raw (copied from ipc.rs for convenience)
#[cfg(target_arch = "x86_64")]
unsafe fn syscall_raw(num: u64, arg1: u64, arg2: u64, arg3: u64, arg4: u64, arg5: u64) -> u64 {
    let ret: u64;
    core::arch::asm!(
        "syscall",
        in("rax") num,
        in("rdi") arg1,
        in("rsi") arg2,
        in("rdx") arg3,
        in("r10") arg4,
        in("r8") arg5,
        out("rax") ret,
        options(nostack, preserves_flags)
    );
    ret
}

#[cfg(not(target_arch = "x86_64"))]
unsafe fn syscall_raw(_num: u64, _arg1: u64, _arg2: u64, _arg3: u64, _arg4: u64, _arg5: u64) -> u64 {
    0
}

// Helper to send IPC messages and get response
fn send_ipc_request(target_port: u64, msg_id: u64, inline_data: &[u8]) -> Result<IpcMessage, ()> {
    let mut msg = IpcMessage {
        sender_tid: 0, // Filled by kernel
        msg_id,
        msg_type: 1, // Request
        inline_size: inline_data.len() as u32,
        inline_data: [0; 64],
        buffer: 0,
        buffer_size: 0,
    };
    msg.inline_data[0..inline_data.len()].copy_from_slice(inline_data);

    unsafe {
        let _ = syscall_raw(SYS_IPC_SEND, target_port, &msg as *const _ as u64, 0, 0, 0);
        let mut response = IpcMessage {
            sender_tid: 0, msg_id: 0, msg_type: 0, inline_size: 0, inline_data: [0; 64], buffer: 0, buffer_size: 0,
        };
        // Blocking receive (timeout could be added)
        let _ = syscall_raw(SYS_IPC_RECEIVE, 0, &mut response as *mut _ as u64, 0, 0, 0); 
        Ok(response)
    }
}

// Function to open a block device via Device Manager
fn open_block_device(device_name: &str) -> Result<u64, ()> {
    let inline_data = device_name.as_bytes();
    let response = send_ipc_request(DRIVER_MANAGER_PORT as u64, DM_MSG_OPEN_DEVICE as u64, inline_data)?;

    if response.msg_id == DM_MSG_OPEN_DEVICE as u64 && response.msg_type == 2 /* Response */ && response.inline_size == 8 {
        let device_handle = u64::from_le_bytes(response.inline_data[0..8].try_into().unwrap());
        if device_handle != 0 {
            Ok(device_handle)
        } else {
            Err(())
        }
    } else {
        Err(())
    }
}

// Get uptime helper
fn get_uptime_ms() -> u64 {
    unsafe { syscall_raw(SYS_GET_UPTIME_MS, 0, 0, 0, 0, 0) }
}

/// SFS Magic number
pub const SFS_MAGIC: u64 = 0x5343415246535F31; // "SCARSF_1" in hex

/// SFS Version
pub const SFS_VERSION_MAJOR: u16 = 1;
pub const SFS_VERSION_MINOR: u16 = 0;

/// Block size (4KB)
pub const BLOCK_SIZE: usize = 4096;

/// Maximum filename length
pub const MAX_FILENAME_LEN: usize = 255;

/// SFS File System structure
pub struct SfsFileSystem {
    /// Superblock
    superblock: Superblock,

    /// Root inode
    root_inode: u64,

    /// Current generation (for CoW)
    current_generation: u64,

    /// CoW manager
    cow_manager: CowManager,

    /// Snapshot manager
    snapshot_manager: SnapshotManager,

    /// Is mounted read-write?
    read_write: bool,

    /// Device handle for block I/O
    device_handle: u64,
}

impl SfsFileSystem {
    /// Create a new SFS instance
    pub fn new() -> Self {
        Self {
            superblock: Superblock::new(),
            root_inode: 0,
            current_generation: 1,
            cow_manager: CowManager::new(),
            snapshot_manager: SnapshotManager::new(),
            read_write: false,
            device_handle: 0,
        }
    }

    /// Format a device with SFS
    pub fn format(device_handle: u64, total_blocks: u64) -> VfsResult<()> {
        let mut superblock = Superblock::new();
        superblock.magic = SFS_MAGIC;
        superblock.version_major = SFS_VERSION_MAJOR;
        superblock.version_minor = SFS_VERSION_MINOR;
        superblock.block_size = BLOCK_SIZE as u32;
        superblock.total_blocks = total_blocks;
        superblock.free_blocks = total_blocks - 1; // Minus superblock
        superblock.total_inodes = total_blocks / 4; // 25% for inodes
        superblock.free_inodes = superblock.total_inodes - 1; // Minus root
        superblock.root_inode = 1;
        superblock.generation = 1;

        // Write superblock to device
        // Implement block I/O via block device driver
        use crate::block_device::write_blocks;
        let superblock_bytes = unsafe {
            core::slice::from_raw_parts(
                &superblock as *const _ as *const u8,
                core::mem::size_of::<Superblock>()
            )
        };
        let mut block_buffer = [0u8; 4096];
        let copy_len = superblock_bytes.len().min(4096);
        block_buffer[0..copy_len].copy_from_slice(&superblock_bytes[0..copy_len]);
        let _ = write_blocks(device_handle as u8, 0, 1, &block_buffer); // Write to block 0, port 0

        Ok(())
    }

    /// Read a block from device
    fn read_block(&self, block_num: u64, buffer: &mut [u8]) -> VfsResult<()> {
        if buffer.len() < BLOCK_SIZE {
            return Err(VfsError::InvalidArgument);
        }

        // Implement block read via device driver IPC
        use crate::block_device::read_blocks;
        // Convert block number to LBA (assuming 4KB blocks, 8 sectors per block)
        let lba = block_num * 8;
        match read_blocks(self.device_handle as u8, lba, 8, buffer) {
            Ok(_) => Ok(()),
            Err(_) => Err(VfsError::IoError),
        }
    }

    /// Write a block to device
    fn write_block(&mut self, block_num: u64, buffer: &[u8]) -> VfsResult<()> {
        if !self.read_write {
            return Err(VfsError::ReadOnly);
        }

        if buffer.len() < BLOCK_SIZE {
            return Err(VfsError::InvalidArgument);
        }

        // Implement block write via device driver IPC
        use crate::block_device::write_blocks;
        // Convert block number to LBA (assuming 4KB blocks, 8 sectors per block)
        let lba = block_num * 8;
        match write_blocks(self.device_handle as u8, lba, 8, buffer) {
            Ok(_) => Ok(()),
            Err(_) => Err(VfsError::IoError),
        }
    }

    /// Allocate a new block (Copy-on-Write)
    fn allocate_block(&mut self) -> VfsResult<u64> {
        if !self.read_write {
            return Err(VfsError::ReadOnly);
        }

        if self.superblock.free_blocks == 0 {
            return Err(VfsError::NoSpace);
        }

        // Implement block allocation with CoW
        // Find a free block (simple bitmap would be better, but for now sequential)
        let block = self.superblock.total_blocks - self.superblock.free_blocks;
        self.superblock.free_blocks -= 1;
        
        // Initialize reference count for CoW
        self.cow_manager.inc_refcount(block);

        Ok(block)
    }

    /// Free a block
    fn free_block(&mut self, block_num: u64) -> VfsResult<()> {
        if !self.read_write {
            return Err(VfsError::ReadOnly);
        }

        // Implement block freeing with reference counting
        let refcount = self.cow_manager.dec_refcount(block_num);
        
        // Only free block if reference count reaches zero
        if refcount == 0 {
            self.superblock.free_blocks += 1;
            // In a full implementation, we would also update the free block bitmap
        }

        Ok(())
    }

    /// Read inode from disk
    fn read_inode(&self, inode_num: u64) -> VfsResult<Inode> {
        // Calculate block containing inode
        let inodes_per_block = BLOCK_SIZE / core::mem::size_of::<Inode>();
        let block = 1 + (inode_num / inodes_per_block as u64); // Block 0 is superblock
        let offset = (inode_num % inodes_per_block as u64) * core::mem::size_of::<Inode>() as u64;

        let mut buffer = [0u8; BLOCK_SIZE];
        self.read_block(block, &mut buffer)?;

        // Parse inode from buffer
        let inode = unsafe {
            core::ptr::read(buffer.as_ptr().add(offset as usize) as *const Inode)
        };

        Ok(inode)
    }

    /// Write inode to disk (CoW)
    fn write_inode(&mut self, inode_num: u64, inode: &Inode) -> VfsResult<()> {
        if !self.read_write {
            return Err(VfsError::ReadOnly);
        }

        // Calculate block containing inode
        let inodes_per_block = BLOCK_SIZE / core::mem::size_of::<Inode>();
        let block = 1 + (inode_num / inodes_per_block as u64);
        let offset = (inode_num % inodes_per_block as u64) * core::mem::size_of::<Inode>() as u64;

        // Read current block
        let mut buffer = [0u8; BLOCK_SIZE];
        self.read_block(block, &mut buffer)?;

        // Copy-on-Write: Allocate new block if needed
        let new_block = if self.cow_manager.is_shared(block) {
            let new_blk = self.allocate_block()?;
            self.write_block(new_blk, &buffer)?;
            self.cow_manager.mark_modified(new_blk);
            new_blk
        } else {
            block
        };

        // Update inode in buffer
        unsafe {
            let ptr = buffer.as_mut_ptr().add(offset as usize) as *mut Inode;
            core::ptr::write(ptr, *inode);
        }

        // Write block
        self.write_block(new_block, &buffer)?;

        Ok(())
    }

    /// Resolve path to inode number
    fn resolve_path(&self, path: &str) -> VfsResult<u64> {
        if path == "/" {
            return Ok(self.root_inode);
        }

        // Split path into components
        let components: Vec<&str> = path.split('/').filter(|s| !s.is_empty()).collect();

        let mut current_inode = self.root_inode;

        for component in components {
            // Read directory inode
            let dir_inode = self.read_inode(current_inode)?;

            if dir_inode.file_type != InodeType::Directory {
                return Err(VfsError::NotDirectory);
            }

            // Search directory for component using linear scan (B-tree not implemented)
            current_inode = self.lookup_dir_entry(current_inode, component)?;
        }

        Ok(current_inode)
    }

    /// Look up directory entry
    fn lookup_dir_entry(&self, dir_inode: u64, name: &str) -> VfsResult<u64> {
        let inode = self.read_inode(dir_inode)?;
        
        if inode.file_type != InodeType::Directory {
            return Err(VfsError::NotDirectory);
        }
        
        // Scan all blocks to find directory entry (B-tree not implemented)
        let num_blocks = (inode.size + BLOCK_SIZE as u64 - 1) / BLOCK_SIZE as u64;
        
        for i in 0..num_blocks {
            let mut buffer = [0u8; BLOCK_SIZE];
            // Use extent tree to find block (placeholder, assumes simple mapping)
            let block_num = if inode.extent_root != 0 {
                inode.extent_root + i
            } else {
                0
            };
            
            if block_num == 0 { continue; }
            
            self.read_block(block_num, &mut buffer)?;
            
            // Iterate entries (assuming fixed 64-byte entries for now)
            const ENTRY_SIZE: usize = 68; // 4 bytes for inode + 64 bytes for name
            const ENTRIES_PER_BLOCK: usize = BLOCK_SIZE / ENTRY_SIZE;
            
            for j in 0..ENTRIES_PER_BLOCK {
                let offset = j * ENTRY_SIZE;
                let entry_inode = u32::from_le_bytes(buffer[offset..offset+4].try_into().unwrap()) as u64;
                
                if entry_inode == 0 { continue; } // Empty entry
                
                // Check name
                let name_bytes = &buffer[offset+4..offset+ENTRY_SIZE];
                let len = name_bytes.iter().position(|&c| c == 0).unwrap_or(64);
                let entry_name = core::str::from_utf8(&name_bytes[0..len])
                    .map_err(|_| VfsError::InvalidData)?;
                
                if entry_name == name {
                    return Ok(entry_inode);
                }
            }
        }
        
        Err(VfsError::NotFound)
    }

    /// Create snapshot
    pub fn create_snapshot(&mut self, name: &str) -> VfsResult<u64> {
        if !self.read_write {
            return Err(VfsError::ReadOnly);
        }

        let snapshot_id = self.snapshot_manager.create_snapshot(
            self.current_generation,
            name,
            self.root_inode,
        )?;

        // Increment generation for CoW
        self.current_generation += 1;
        self.superblock.generation = self.current_generation;

        Ok(snapshot_id)
    }

    /// Rollback to snapshot
    pub fn rollback_to_snapshot(&mut self, snapshot_id: u64) -> VfsResult<()> {
        if !self.read_write {
            return Err(VfsError::ReadOnly);
        }

        let snapshot = self.snapshot_manager.get_snapshot(snapshot_id)?;

        // Restore root inode from snapshot
        self.root_inode = snapshot.root_inode;
        self.current_generation = snapshot.generation + 1;
        self.superblock.generation = self.current_generation;

        Ok(())
    }
}

impl FileSystemOps for SfsFileSystem {
    fn mount(&mut self, device: &str, flags: u32) -> VfsResult<()> {
        // Open device via device manager
        let device_handle = open_block_device(device)
            .map_err(|_| VfsError::DeviceNotFound)?;
        self.device_handle = device_handle;

        // Read superblock
        let mut buffer = [0u8; BLOCK_SIZE];
        self.read_block(0, &mut buffer)?;

        let superblock = unsafe {
            core::ptr::read(buffer.as_ptr() as *const Superblock)
        };

        // Verify magic
        if superblock.magic != SFS_MAGIC {
            return Err(VfsError::InvalidArgument);
        }

        self.superblock = superblock;
        self.root_inode = superblock.root_inode;
        self.current_generation = superblock.generation;
        self.read_write = (flags & 0x01) != 0; // Check read-write flag

        Ok(())
    }

    fn unmount(&mut self) -> VfsResult<()> {
        // Sync all pending writes
        self.sync()?;

        // Close device
        // Device handle is just a port index, no explicit close needed
        // In a full implementation, we would notify device manager
        self.device_handle = 0;

        Ok(())
    }

    fn open(&mut self, path: &str, flags: u32, mode: u16) -> VfsResult<u64> {
        // Resolve path to inode
        let inode_num = match self.resolve_path(path) {
            Ok(num) => num,
            Err(VfsError::NotFound) if (flags & O_CREAT) != 0 => {
                // Create new file
                // Implement file creation
                // Allocate new inode
                if self.superblock.free_inodes == 0 {
                    return Err(VfsError::NoSpace);
                }
                let new_inode_num = self.superblock.total_inodes - self.superblock.free_inodes;
                self.superblock.free_inodes -= 1;
                
                // Create new inode
                let mut new_inode = Inode::new();
                new_inode.file_type = InodeType::RegularFile;
                new_inode.mode = mode as u16;
                new_inode.size = 0;
                new_inode.blocks = 0;
                
                // Write inode
                self.write_inode(new_inode_num, &new_inode)?;
                
                // Add to parent directory (would use B-tree)
                // For now, just return the inode number
                return Ok(new_inode_num);
            }
            Err(e) => return Err(e),
        };

        // Return inode number as file handle
        Ok(inode_num)
    }

    fn close(&mut self, file_handle: u64) -> VfsResult<()> {
        // Nothing to do for now
        Ok(())
    }

    fn read(&mut self, file_handle: u64, buffer: &mut [u8], offset: u64) -> VfsResult<usize> {
        // Read inode
        let inode = self.read_inode(file_handle)?;

        if inode.file_type != InodeType::RegularFile && inode.file_type != InodeType::Directory {
            return Err(VfsError::InvalidArgument);
        }

        // Calculate block and offset
        let block_idx = offset / BLOCK_SIZE as u64;
        let block_offset = (offset % BLOCK_SIZE as u64) as usize;
        let mut bytes_read = 0;

        // Read data blocks using extent tree
        // For now, use extent_root to find block
        // Full implementation would traverse B-tree extent tree
        if inode.extent_root != 0 {
            // Use B-tree to find block number for this block_idx
            // For now, simple calculation (full implementation would query B-tree)
            // This is a placeholder - real implementation would:
            // 1. Query extent B-tree with key=block_idx
            // 2. Get block number from extent
            // 3. Read block
            let block_num = inode.extent_root + block_idx; // Placeholder
            if block_num != 0 {
                let mut block_data = [0u8; BLOCK_SIZE];
                self.read_block(block_num, &mut block_data)?;
                
                let copy_len = buffer.len().min(BLOCK_SIZE - block_offset);
                buffer[0..copy_len].copy_from_slice(&block_data[block_offset..block_offset + copy_len]);
                bytes_read = copy_len;
            }
        } else if inode.size > 0 && inode.size <= 60 {
            // Use inline data for small files
            let copy_len = buffer.len().min((inode.size - offset) as usize);
            buffer[0..copy_len].copy_from_slice(&inode.inline_data[offset as usize..offset as usize + copy_len]);
            bytes_read = copy_len;
        }
        Ok(bytes_read)
    }

    fn write(&mut self, file_handle: u64, buffer: &[u8], offset: u64) -> VfsResult<usize> {
        if !self.read_write {
            return Err(VfsError::ReadOnly);
        }

        // Read inode
        let mut inode = self.read_inode(file_handle)?;

        if inode.file_type != InodeType::RegularFile {
            return Err(VfsError::InvalidArgument);
        }

        // Write data using CoW
        // Calculate block and offset
        let block_idx = offset / BLOCK_SIZE as u64;
        let block_offset = (offset % BLOCK_SIZE as u64) as usize;
        let mut bytes_written = 0;
        
        // Allocate block if needed (with CoW)
        let block_num = if inode.extent_root == 0 {
            // Allocate first block
            let new_block = self.allocate_block()?;
            inode.extent_root = new_block;
            new_block
        } else {
            // Find or allocate block for this block_idx
            // Full implementation would query/extend extent tree
            inode.extent_root + block_idx // Placeholder
        };
        
        // Read existing block (for CoW)
        let mut block_data = [0u8; BLOCK_SIZE];
        if self.cow_manager.is_shared(block_num) {
            // Copy-on-Write: allocate new block
            let new_block = self.allocate_block()?;
            self.read_block(block_num, &mut block_data)?;
            self.write_block(new_block, &block_data)?;
            // Update extent tree would go here
        } else {
            self.read_block(block_num, &mut block_data)?;
        }
        
        // Write data to block
        let copy_len = buffer.len().min(BLOCK_SIZE - block_offset);
        block_data[block_offset..block_offset + copy_len].copy_from_slice(&buffer[0..copy_len]);
        self.write_block(block_num, &block_data)?;
        
        // Update inode
        inode.size = inode.size.max(offset + copy_len as u64);
        inode.mtime = get_uptime_ms();
        self.write_inode(file_handle, &inode)?;
        
        bytes_written = copy_len;
        Ok(bytes_written)
    }

    fn stat(&self, path: &str) -> VfsResult<FileStat> {
        let inode_num = self.resolve_path(path)?;
        let inode = self.read_inode(inode_num)?;

        Ok(FileStat {
            file_type: match inode.file_type {
                InodeType::RegularFile => FileType::Regular,
                InodeType::Directory => FileType::Directory,
                InodeType::Symlink => FileType::Symlink,
                _ => FileType::Unknown,
            },
            size: inode.size,
            blocks: inode.blocks,
            block_size: BLOCK_SIZE as u32,
            inode: inode_num,
            links: inode.links as u32,
            uid: inode.uid,
            gid: inode.gid,
            mode: inode.mode,
            atime: inode.atime,
            mtime: inode.mtime,
            ctime: inode.ctime,
        })
    }

    fn fstat(&self, file_handle: u64) -> VfsResult<FileStat> {
        let inode = self.read_inode(file_handle)?;

        Ok(FileStat {
            file_type: match inode.file_type {
                InodeType::RegularFile => FileType::Regular,
                InodeType::Directory => FileType::Directory,
                InodeType::Symlink => FileType::Symlink,
                _ => FileType::Unknown,
            },
            size: inode.size,
            blocks: inode.blocks,
            block_size: BLOCK_SIZE as u32,
            inode: file_handle,
            links: inode.links as u32,
            uid: inode.uid,
            gid: inode.gid,
            mode: inode.mode,
            atime: inode.atime,
            mtime: inode.mtime,
            ctime: inode.ctime,
        })
    }

    fn mkdir(&mut self, path: &str, mode: u16) -> VfsResult<()> {
        if !self.read_write {
            return Err(VfsError::ReadOnly);
        }

        // Implement directory creation
        // Allocate new inode for directory
        if self.superblock.free_inodes == 0 {
            return Err(VfsError::NoSpace);
        }
        let new_inode_num = self.superblock.total_inodes - self.superblock.free_inodes;
        self.superblock.free_inodes -= 1;
        
        // Create directory inode
        let mut dir_inode = Inode::new();
        dir_inode.file_type = InodeType::Directory;
        dir_inode.mode = mode;
        dir_inode.size = 0;
        dir_inode.blocks = 0;
        dir_inode.ctime = get_uptime_ms();
        dir_inode.mtime = get_uptime_ms();
        
        // Write inode
        self.write_inode(new_inode_num, &dir_inode)?;
        
        // Add "." and ".." entries (would use B-tree for directory entries)
        // For now, directory is created but entries not added
        // Full implementation would add directory entries via B-tree
        
        Ok(())
    }

    fn rmdir(&mut self, path: &str) -> VfsResult<()> {
        if !self.read_write {
            return Err(VfsError::ReadOnly);
        }

        // Implement directory removal
        let inode_num = self.resolve_path(path)?;
        let inode = self.read_inode(inode_num)?;
        
        if inode.file_type != InodeType::Directory {
            return Err(VfsError::NotDirectory);
        }
        
        // Check if directory is empty (would check B-tree for entries)
        // For now, just check if size is 0 (only "." and ".." would be present)
        if inode.size > 0 {
            return Err(VfsError::NotEmpty);
        }
        
        // Free inode
        self.superblock.free_inodes += 1;
        // In full implementation, would also free blocks and update B-tree
        
        Ok(())
    }

    fn unlink(&mut self, path: &str) -> VfsResult<()> {
        if !self.read_write {
            return Err(VfsError::ReadOnly);
        }

        // Implement file removal
        let inode_num = self.resolve_path(path)?;
        let mut inode = self.read_inode(inode_num)?;
        
        if inode.file_type == InodeType::Directory {
            return Err(VfsError::IsDirectory);
        }
        
        // Decrement link count
        if inode.links > 0 {
            inode.links -= 1;
        }
        
        // If no more links, free blocks and inode
        if inode.links == 0 {
            // Free blocks (would traverse extent tree)
            // For now, just free the inode
            self.superblock.free_inodes += 1;
            // In full implementation, would free all blocks via CoW reference counting
        } else {
            // Update inode
            self.write_inode(inode_num, &inode)?;
        }
        
        // Remove from parent directory (would use B-tree)
        // For now, just mark as removed
        
        Ok(())
    }

    fn rename(&mut self, old_path: &str, new_path: &str) -> VfsResult<()> {
        if !self.read_write {
            return Err(VfsError::ReadOnly);
        }

        // Implement rename
        let inode_num = self.resolve_path(old_path)?;
        
        // Remove old name from parent directory
        // Add new name to new parent directory
        // Both operations would use B-tree directory entries
        // For now, just verify paths are valid
        let _old_inode = self.read_inode(inode_num)?;
        
        // In full implementation:
        // 1. Parse old_path and new_path to get parent directories
        // 2. Remove entry from old parent's B-tree
        // 3. Add entry to new parent's B-tree
        // 4. Update inode if directory moved
        
        Ok(())
    }

    fn opendir(&mut self, path: &str) -> VfsResult<u64> {
        let inode_num = self.resolve_path(path)?;
        let inode = self.read_inode(inode_num)?;

        if inode.file_type != InodeType::Directory {
            return Err(VfsError::NotDirectory);
        }

        Ok(inode_num)
    }

    fn readdir(&mut self, dir_handle: u64) -> VfsResult<Option<DirEntry>> {
        // Implement directory reading
        // Would use B-tree to iterate directory entries
        // For now, return None (no entries)
        // Full implementation would:
        // 1. Query directory's B-tree
        // 2. Return next entry
        // 3. Track position for subsequent calls
        let _inode = self.read_inode(dir_handle)?;
        Ok(None)
    }

    fn closedir(&mut self, dir_handle: u64) -> VfsResult<()> {
        Ok(())
    }

    fn truncate(&mut self, path: &str, size: u64) -> VfsResult<()> {
        if !self.read_write {
            return Err(VfsError::ReadOnly);
        }

        // Implement truncate
        let inode_num = self.resolve_path(path)?;
        let mut inode = self.read_inode(inode_num)?;
        
        if inode.file_type != InodeType::RegularFile {
            return Err(VfsError::InvalidArgument);
        }
        
        // Update size
        let old_size = inode.size;
        inode.size = size;
        
        // If truncating to smaller size, free blocks
        if size < old_size {
            // Calculate blocks to free
            let old_blocks = (old_size + BLOCK_SIZE as u64 - 1) / BLOCK_SIZE as u64;
            let new_blocks = (size + BLOCK_SIZE as u64 - 1) / BLOCK_SIZE as u64;
            
            // Free blocks beyond new size (would traverse extent tree)
            for block_idx in new_blocks..old_blocks {
                // Get block number from extent tree and free it
                // For now, just update inode
            }
        }
        
        // Update inode
        inode.mtime = get_uptime_ms();
        self.write_inode(inode_num, &inode)?;
        
        Ok(())
    }

    fn sync(&mut self) -> VfsResult<()> {
        if !self.read_write {
            return Ok(());
        }

        // Write superblock
        let mut buffer = [0u8; BLOCK_SIZE];
        unsafe {
            let ptr = buffer.as_mut_ptr() as *mut Superblock;
            core::ptr::write(ptr, self.superblock);
        }
        self.write_block(0, &buffer)?;

        Ok(())
    }
}