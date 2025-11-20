//! FAT32 Filesystem Driver
//! 
//! User-space FAT32 filesystem driver that communicates with block devices
//! via IPC and registers with the VFS service.

#![no_std]

pub mod fat32;
pub mod block;
pub mod ipc;

pub use fat32::*;
pub use block::*;
pub use ipc::*;

