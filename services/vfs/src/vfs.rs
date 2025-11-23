//! VFS implementation for VFS service

use core::mem;

/// File descriptor entry
#[repr(C)]
pub struct FdEntry {
    pub used: bool,
    pub fs_id: u64,
    pub file_data: u64,  // Opaque pointer
    pub position: u64,
    pub flags: u64,
}

/// Mount point
#[repr(C)]
pub struct MountPoint {
    pub mountpoint: [u8; 256],
    pub fs_id: u64,
    pub device: [u8; 256],
    pub next: u64,  // Pointer to next mount
}

const MAX_FDS: usize = 256;
const MAX_MOUNTS: usize = 32;

static mut FD_TABLE: [FdEntry; MAX_FDS] = unsafe { mem::zeroed() };
static mut MOUNT_POINTS: [MountPoint; MAX_MOUNTS] = unsafe { mem::zeroed() };
static mut MOUNT_COUNT: usize = 0;
static mut ROOT_MOUNT: usize = 0;

/// Initialize VFS
pub fn vfs_init() -> Result<(), ()> {
    unsafe {
        // Clear FD table
        for i in 0..MAX_FDS {
            FD_TABLE[i].used = false;
            FD_TABLE[i].fs_id = 0;
            FD_TABLE[i].file_data = 0;
            FD_TABLE[i].position = 0;
            FD_TABLE[i].flags = 0;
        }
        
        MOUNT_COUNT = 0;
        ROOT_MOUNT = 0;
        
        Ok(())
    }
}

/// Allocate file descriptor
pub fn allocate_fd() -> Option<i32> {
    unsafe {
        for i in 0..MAX_FDS {
            if !FD_TABLE[i].used {
                FD_TABLE[i].used = true;
                return Some(i as i32);
            }
        }
        None
    }
}

/// Free file descriptor
pub fn free_fd(fd: i32) {
    unsafe {
        if fd >= 0 && (fd as usize) < MAX_FDS {
            FD_TABLE[fd as usize].used = false;
        }
    }
}

/// Get file descriptor entry
pub fn get_fd_entry(fd: i32) -> Option<&'static mut FdEntry> {
    unsafe {
        if fd >= 0 && (fd as usize) < MAX_FDS && FD_TABLE[fd as usize].used {
            Some(&mut FD_TABLE[fd as usize])
        } else {
            None
        }
    }
}

/// Mount filesystem
pub fn vfs_mount(device: &[u8], mountpoint: &[u8], fs_type: &[u8]) -> Result<(), ()> {
    unsafe {
        if MOUNT_COUNT >= MAX_MOUNTS {
            return Err(());
        }
        
        let mount = &mut MOUNT_POINTS[MOUNT_COUNT];
        
        // Copy mountpoint
        let mnt_len = mountpoint.len().min(255);
        mount.mountpoint[0..mnt_len].copy_from_slice(&mountpoint[0..mnt_len]);
        mount.mountpoint[mnt_len] = 0;
        
        // Copy device
        let dev_len = device.len().min(255);
        mount.device[0..dev_len].copy_from_slice(&device[0..dev_len]);
        mount.device[dev_len] = 0;
        
        // Look up filesystem type and get fs_id
        // Filesystem type mapping:
        // "sfs" -> 1 (Scarlett File System)
        // "fat32" -> 2
        // "ext4" -> 3
        // "ntfs" -> 4
        let fstype_str = core::str::from_utf8(fs_type).unwrap_or("");
        mount.fs_id = if fstype_str == "sfs" {
            1
        } else if fstype_str == "fat32" {
            2
        } else if fstype_str == "ext4" {
            3
        } else if fstype_str == "ntfs" {
            4
        } else {
            0  // Unknown filesystem type
        };
        
        // If mounting at root, set as root mount
        if mountpoint.len() == 1 && mountpoint[0] == b'/' {
            ROOT_MOUNT = MOUNT_COUNT;
        }
        
        MOUNT_COUNT += 1;
        Ok(())
    }
}

/// Resolve path to mount point
pub fn resolve_path(path: &[u8]) -> Option<usize> {
    unsafe {
        // Implement proper path resolution
        // Find the mount point that matches the longest prefix of the path
        let mut best_match: Option<usize> = None;
        let mut best_match_len = 0;
        
        // Convert path to string for comparison
        let path_str = core::str::from_utf8(path).unwrap_or("");
        
        for i in 0..MOUNT_COUNT {
            let mount = &MOUNT_POINTS[i];
            let mnt_str = core::str::from_utf8(&mount.mountpoint[..mount.mountpoint.iter().position(|&b| b == 0).unwrap_or(0)]).unwrap_or("");
            
            // Check if path starts with mountpoint
            if path_str.starts_with(mnt_str) && mnt_str.len() > best_match_len {
                best_match = Some(i);
                best_match_len = mnt_str.len();
            }
        }
        
        // If no match found, use root mount
        best_match.or_else(|| {
            if ROOT_MOUNT < MOUNT_COUNT {
                Some(ROOT_MOUNT)
            } else {
                None
            }
        })
    }
}

/// Get filesystem ID for a mount point
pub fn get_mount_fs_id(mount_idx: usize) -> u64 {
    unsafe {
        if mount_idx < MOUNT_COUNT {
            MOUNT_POINTS[mount_idx].fs_id
        } else {
            0
        }
    }
}

