//! VFS Service Library

pub mod vfs;
pub mod block_device;
pub mod syscalls;

pub use crate::ipc::{IpcMessage, IPC_MSG_REQUEST, IPC_MSG_RESPONSE};
use vfs::{vfs_init, vfs_mount, allocate_fd, free_fd, get_fd_entry, resolve_path};

/// VFS service port
static mut SERVICE_PORT: u64 = 0;
static mut INITIALIZED: bool = false;

/// VFS operation types
pub const VFS_OP_OPEN: u64 = 1;
pub const VFS_OP_READ: u64 = 2;
pub const VFS_OP_WRITE: u64 = 3;
pub const VFS_OP_CLOSE: u64 = 4;
pub const VFS_OP_STAT: u64 = 5;
pub const VFS_OP_READDIR: u64 = 6;
pub const VFS_OP_MOUNT: u64 = 7;
pub const VFS_OP_UNMOUNT: u64 = 8;

/// Initialize VFS IPC
pub fn init_ipc() -> Result<u64, ()> {
    unsafe {
        if SERVICE_PORT == 0 {
            // Create IPC port via syscall
            SERVICE_PORT = sys_ipc_create_port();
            if SERVICE_PORT == 0 {
                return Err(());
            }
        }
        Ok(SERVICE_PORT)
    }
}

/// System call to create IPC port
#[no_mangle]
pub extern "C" fn sys_ipc_create_port() -> u64 {
    unsafe {
        syscall_raw(26, 0, 0, 0, 0, 0)
    }
}

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

/// Initialize VFS service
pub fn init() -> Result<(), ()> {
    unsafe {
        if INITIALIZED {
            return Ok(());
        }
        
        // Initialize VFS
        vfs_init()?;
        
        // TODO: Mount root filesystem
        // For now, just mark as initialized
        
        INITIALIZED = true;
        Ok(())
    }
}

/// Handle file open request
pub fn handle_open(request: &IpcMessage) -> IpcMessage {
    let mut response = IpcMessage::new();
    response.msg_type = IPC_MSG_RESPONSE;
    response.msg_id = request.msg_id;
    
    // Parse path from request inline data
    if request.inline_size > 0 {
        let path_len = request.inline_size as usize;
        let path = &request.inline_data[0..path_len.min(64)];
        
        // Resolve path to mount point
        if let Some(_mount_idx) = resolve_path(path) {
            // Allocate file descriptor
            if let Some(fd) = allocate_fd() {
                // TODO: Call filesystem open function
                // For now, just return the FD
                response.inline_data[0..4].copy_from_slice(&fd.to_le_bytes());
                response.inline_size = 4;
            } else {
                // Out of file descriptors
                response.inline_data[0] = 0xFF;  // Error
                response.inline_size = 1;
            }
        } else {
            // Path resolution failed
            response.inline_data[0] = 0xFE;  // Error
            response.inline_size = 1;
        }
    }
    
    response
}

/// Handle file read request
pub fn handle_read(request: &IpcMessage) -> IpcMessage {
    let mut response = IpcMessage::new();
    response.msg_type = IPC_MSG_RESPONSE;
    response.msg_id = request.msg_id;
    
    // Parse fd and count from request
    if request.inline_size >= 8 {
        let fd = i32::from_le_bytes([
            request.inline_data[0],
            request.inline_data[1],
            request.inline_data[2],
            request.inline_data[3],
        ]);
        let count = u32::from_le_bytes([
            request.inline_data[4],
            request.inline_data[5],
            request.inline_data[6],
            request.inline_data[7],
        ]) as usize;
        
        if let Some(fd_entry) = get_fd_entry(fd) {
            // TODO: Call filesystem read function
            // For now, return 0 bytes read
            let bytes_read: u32 = 0;
            response.inline_data[0..4].copy_from_slice(&bytes_read.to_le_bytes());
            response.inline_size = 4;
        } else {
            // Invalid file descriptor
            response.inline_data[0] = 0xFF;
            response.inline_size = 1;
        }
    }
    
    response
}

/// Handle file write request
pub fn handle_write(request: &IpcMessage) -> IpcMessage {
    let mut response = IpcMessage::new();
    response.msg_type = IPC_MSG_RESPONSE;
    response.msg_id = request.msg_id;
    
    // Parse fd from request
    if request.inline_size >= 4 {
        let fd = i32::from_le_bytes([
            request.inline_data[0],
            request.inline_data[1],
            request.inline_data[2],
            request.inline_data[3],
        ]);
        
        if let Some(_fd_entry) = get_fd_entry(fd) {
            // TODO: Call filesystem write function
            // Data would be in request.buffer
            // For now, return 0 bytes written
            let bytes_written: u32 = 0;
            response.inline_data[0..4].copy_from_slice(&bytes_written.to_le_bytes());
            response.inline_size = 4;
        } else {
            // Invalid file descriptor
            response.inline_data[0] = 0xFF;
            response.inline_size = 1;
        }
    }
    
    response
}

/// Handle file close request
pub fn handle_close(request: &IpcMessage) -> IpcMessage {
    let mut response = IpcMessage::new();
    response.msg_type = IPC_MSG_RESPONSE;
    response.msg_id = request.msg_id;
    
    // Parse fd from request
    if request.inline_size >= 4 {
        let fd = i32::from_le_bytes([
            request.inline_data[0],
            request.inline_data[1],
            request.inline_data[2],
            request.inline_data[3],
        ]);
        
        // TODO: Call filesystem close function
        free_fd(fd);
        
        response.inline_data[0] = 0;  // Success
        response.inline_size = 1;
    }
    
    response
}

/// Handle mount request
pub fn handle_mount(request: &IpcMessage) -> IpcMessage {
    let mut response = IpcMessage::new();
    response.msg_type = IPC_MSG_RESPONSE;
    response.msg_id = request.msg_id;
    
    // TODO: Parse device, mountpoint, fstype from request
    // For now, return success
    response.inline_data[0] = 0;  // Success
    response.inline_size = 1;
    
    response
}

