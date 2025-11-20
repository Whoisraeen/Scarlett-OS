//! SFS Inode Structure

/// Inode type
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum InodeType {
    Unknown = 0,
    RegularFile = 1,
    Directory = 2,
    Symlink = 3,
    CharDevice = 4,
    BlockDevice = 5,
    Fifo = 6,
    Socket = 7,
}

/// SFS Inode
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct Inode {
    /// File type
    pub file_type: InodeType,

    /// Permissions and mode
    pub mode: u16,

    /// Owner user ID
    pub uid: u32,

    /// Owner group ID
    pub gid: u32,

    /// File size in bytes
    pub size: u64,

    /// Number of blocks
    pub blocks: u64,

    /// Number of hard links
    pub links: u16,

    /// Access time
    pub atime: u64,

    /// Modification time
    pub mtime: u64,

    /// Creation time
    pub ctime: u64,

    /// Generation number (for CoW)
    pub generation: u64,

    /// Extent tree root
    pub extent_root: u64,

    /// Inline data (for small files)
    pub inline_data: [u8; 60],

    /// Flags
    pub flags: u32,

    /// Reference count (for CoW and dedup)
    pub refcount: u32,

    /// Compression algorithm
    pub compression: u8,

    /// Encryption status
    pub encrypted: bool,

    /// Reserved
    pub _reserved: [u8; 30],
}

impl Inode {
    pub fn new() -> Self {
        Self {
            file_type: InodeType::RegularFile,
            mode: 0o644,
            uid: 0,
            gid: 0,
            size: 0,
            blocks: 0,
            links: 1,
            atime: 0,
            mtime: 0,
            ctime: 0,
            generation: 1,
            extent_root: 0,
            inline_data: [0; 60],
            flags: 0,
            refcount: 1,
            compression: 0,
            encrypted: false,
            _reserved: [0; 30],
        }
    }

    pub fn is_regular_file(&self) -> bool {
        self.file_type == InodeType::RegularFile
    }

    pub fn is_directory(&self) -> bool {
        self.file_type == InodeType::Directory
    }

    pub fn is_symlink(&self) -> bool {
        self.file_type == InodeType::Symlink
    }
}
