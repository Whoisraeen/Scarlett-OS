//! Virtual File System Service
//! 
//! This service runs in user-space and provides file system operations
//! via IPC to other processes.

#![no_std]
#![no_main]

mod ipc;
mod lib;
mod block_device;

use core::panic::PanicInfo;
use lib::{init_ipc, init, handle_open, handle_read, handle_write, handle_close, handle_mount, 
          VFS_OP_OPEN, VFS_OP_READ, VFS_OP_WRITE, VFS_OP_CLOSE, VFS_OP_MOUNT};
use ipc::{IpcMessage, sys_ipc_receive, sys_ipc_send};
use block_device::{set_block_device_port, read_blocks, write_blocks};

/// Panic handler for the VFS service
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

/// Entry point for the VFS service
#[no_mangle]
pub extern "C" fn _start() -> ! {
    vfs_init();
    vfs_loop();
}

/// Initialize the VFS service
fn vfs_init() {
    // Initialize IPC
    if let Ok(port) = init_ipc() {
        // Register with device manager
        // TODO: In real implementation, send registration message to device manager
        // For now, we'll discover the block device driver port when needed
        
        // Initialize VFS
        let _ = init();
    }
}

/// Main service loop - handles file system requests via IPC
fn vfs_loop() {
    let mut msg = IpcMessage::new();
    
    loop {
        // Receive IPC message
        if sys_ipc_receive(2, &mut msg) == 0 {
            let response = match msg.msg_id {
                VFS_OP_OPEN => handle_open(&msg),
                VFS_OP_READ => handle_read(&msg),
                VFS_OP_WRITE => handle_write(&msg),
                VFS_OP_CLOSE => handle_close(&msg),
                VFS_OP_MOUNT => handle_mount(&msg),
                _ => {
                    // Unknown operation
                    let mut resp = IpcMessage::new();
                    resp.msg_type = ipc::IPC_MSG_RESPONSE;
                    resp.msg_id = msg.msg_id;
                    resp.inline_data[0] = 0xFF;  // Error
                    resp.inline_size = 1;
                    resp
                }
            };

            // Send response back to sender on their reply port (using sender_tid as a proxy)
            // In a fuller implementation, we would resolve sender_tid to a reply port.
            let reply_port = msg.sender_tid;
            let _ = sys_ipc_send(reply_port, &response);
        }
    }
}

