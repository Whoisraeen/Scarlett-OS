//! File Operations Interface
//!
//! Defines the standard interface for file system operations.

use core::fmt;

/// File open modes
pub const O_RDONLY: u32 = 0x0000;
pub const O_WRONLY: u32 = 0x0001;
pub const O_RDWR: u32 = 0x0002;
pub const O_CREAT: u32 = 0x0040;
pub const O_EXCL: u32 = 0x0080;
pub const O_TRUNC: u32 = 0x0200;
pub const O_APPEND: u32 = 0x0400;
pub const O_DIRECTORY: u32 = 0x10000;

/// Seek modes
pub const SEEK_SET: i32 = 0;
pub const SEEK_CUR: i32 = 1;
pub const SEEK_END: i32 = 2;

/// File types
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum FileType {
    Unknown = 0,
    Regular = 1,
    Directory = 2,
    Symlink = 3,
    CharDevice = 4,
    BlockDevice = 5,
    Fifo = 6,
    Socket = 7,
}

/// File stat structure
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FileStat {
    pub file_type: FileType,
    pub size: u64,
    pub blocks: u64,
    pub block_size: u32,
    pub inode: u64,
    pub links: u32,
    pub uid: u32,
    pub gid: u32,
    pub mode: u16,
    pub atime: u64,  // Access time
    pub mtime: u64,  // Modification time
    pub ctime: u64,  // Creation time
}

/// Directory entry
#[repr(C)]
pub struct DirEntry {
    pub inode: u64,
    pub file_type: FileType,
    pub name_len: u16,
    pub name: [u8; 256],
}

impl DirEntry {
    pub fn new() -> Self {
        Self {
            inode: 0,
            file_type: FileType::Unknown,
            name_len: 0,
            name: [0; 256],
        }
    }

    pub fn get_name(&self) -> &[u8] {
        &self.name[0..self.name_len as usize]
    }
}

/// VFS Error types
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VfsError {
    NotFound,
    PermissionDenied,
    AlreadyExists,
    InvalidArgument,
    IoError,
    NotDirectory,
    IsDirectory,
    NotEmpty,
    NoSpace,
    NameTooLong,
    ReadOnly,
    NotSupported,
    InvalidFd,
    TooManyOpenFiles,
}

impl fmt::Display for VfsError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            VfsError::NotFound => write!(f, "Not found"),
            VfsError::PermissionDenied => write!(f, "Permission denied"),
            VfsError::AlreadyExists => write!(f, "Already exists"),
            VfsError::InvalidArgument => write!(f, "Invalid argument"),
            VfsError::IoError => write!(f, "I/O error"),
            VfsError::NotDirectory => write!(f, "Not a directory"),
            VfsError::IsDirectory => write!(f, "Is a directory"),
            VfsError::NotEmpty => write!(f, "Directory not empty"),
            VfsError::NoSpace => write!(f, "No space left on device"),
            VfsError::NameTooLong => write!(f, "Name too long"),
            VfsError::ReadOnly => write!(f, "Read-only file system"),
            VfsError::NotSupported => write!(f, "Operation not supported"),
            VfsError::InvalidFd => write!(f, "Invalid file descriptor"),
            VfsError::TooManyOpenFiles => write!(f, "Too many open files"),
        }
    }
}

pub type VfsResult<T> = Result<T, VfsError>;

/// File system operations trait
pub trait FileSystemOps {
    /// Mount the file system
    fn mount(&mut self, device: &str, flags: u32) -> VfsResult<()>;

    /// Unmount the file system
    fn unmount(&mut self) -> VfsResult<()>;

    /// Open a file
    fn open(&mut self, path: &str, flags: u32, mode: u16) -> VfsResult<u64>;

    /// Close a file
    fn close(&mut self, file_handle: u64) -> VfsResult<()>;

    /// Read from a file
    fn read(&mut self, file_handle: u64, buffer: &mut [u8], offset: u64) -> VfsResult<usize>;

    /// Write to a file
    fn write(&mut self, file_handle: u64, buffer: &[u8], offset: u64) -> VfsResult<usize>;

    /// Get file status
    fn stat(&self, path: &str) -> VfsResult<FileStat>;

    /// Get file status by handle
    fn fstat(&self, file_handle: u64) -> VfsResult<FileStat>;

    /// Create directory
    fn mkdir(&mut self, path: &str, mode: u16) -> VfsResult<()>;

    /// Remove directory
    fn rmdir(&mut self, path: &str) -> VfsResult<()>;

    /// Remove file
    fn unlink(&mut self, path: &str) -> VfsResult<()>;

    /// Rename file
    fn rename(&mut self, old_path: &str, new_path: &str) -> VfsResult<()>;

    /// Open directory for reading
    fn opendir(&mut self, path: &str) -> VfsResult<u64>;

    /// Read directory entry
    fn readdir(&mut self, dir_handle: u64) -> VfsResult<Option<DirEntry>>;

    /// Close directory
    fn closedir(&mut self, dir_handle: u64) -> VfsResult<()>;

    /// Truncate file
    fn truncate(&mut self, path: &str, size: u64) -> VfsResult<()>;

    /// Sync file system
    fn sync(&mut self) -> VfsResult<()>;
}

/// File handle structure
pub struct FileHandle {
    pub file_data: u64,
    pub offset: u64,
    pub flags: u32,
    pub fs_id: u64,
}

impl FileHandle {
    pub fn new(file_data: u64, flags: u32, fs_id: u64) -> Self {
        Self {
            file_data,
            offset: 0,
            flags,
            fs_id,
        }
    }

    pub fn seek(&mut self, offset: i64, whence: i32, file_size: u64) -> VfsResult<u64> {
        let new_offset = match whence {
            SEEK_SET => offset.max(0) as u64,
            SEEK_CUR => {
                if offset >= 0 {
                    self.offset + offset as u64
                } else {
                    self.offset.saturating_sub((-offset) as u64)
                }
            }
            SEEK_END => {
                if offset >= 0 {
                    file_size + offset as u64
                } else {
                    file_size.saturating_sub((-offset) as u64)
                }
            }
            _ => return Err(VfsError::InvalidArgument),
        };

        self.offset = new_offset;
        Ok(new_offset)
    }
}
