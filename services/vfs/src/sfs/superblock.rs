//! SFS Superblock Structure

use super::SFS_MAGIC;

/// SFS Superblock
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct Superblock {
    /// Magic number
    pub magic: u64,

    /// Version (major.minor)
    pub version_major: u16,
    pub version_minor: u16,

    /// Block size in bytes
    pub block_size: u32,

    /// Total number of blocks
    pub total_blocks: u64,

    /// Free blocks
    pub free_blocks: u64,

    /// Total inodes
    pub total_inodes: u64,

    /// Free inodes
    pub free_inodes: u64,

    /// Root inode number
    pub root_inode: u64,

    /// Current generation (for CoW)
    pub generation: u64,

    /// UUID
    pub uuid: [u8; 16],

    /// Volume name
    pub volume_name: [u8; 64],

    /// Mount count
    pub mount_count: u32,

    /// Maximum mount count before fsck
    pub max_mount_count: u32,

    /// Last mount time
    pub last_mount_time: u64,

    /// Last write time
    pub last_write_time: u64,

    /// Last check time
    pub last_check_time: u64,

    /// Filesystem state flags
    pub state: u32,

    /// Snapshot root inode
    pub snapshot_root: u64,

    /// Deduplication enabled
    pub dedup_enabled: bool,

    /// Compression enabled
    pub compression_enabled: bool,

    /// Padding to 4KB
    pub _reserved: [u8; 3806],
}

impl Superblock {
    pub fn new() -> Self {
        Self {
            magic: SFS_MAGIC,
            version_major: 1,
            version_minor: 0,
            block_size: 4096,
            total_blocks: 0,
            free_blocks: 0,
            total_inodes: 0,
            free_inodes: 0,
            root_inode: 1,
            generation: 1,
            uuid: [0; 16],
            volume_name: [0; 64],
            mount_count: 0,
            max_mount_count: 50,
            last_mount_time: 0,
            last_write_time: 0,
            last_check_time: 0,
            state: 0,
            snapshot_root: 0,
            dedup_enabled: true,
            compression_enabled: true,
            _reserved: [0; 3806],
        }
    }
}
