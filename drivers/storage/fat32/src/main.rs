//! FAT32 Filesystem Driver
//! 
//! User-space FAT32 filesystem driver

#![no_std]
#![no_main]

use core::panic::PanicInfo;
use core::convert::TryInto;

extern crate alloc;
use alloc::vec::Vec;
use alloc::string::String;

use driver_framework::ipc::{ipc_create_port, ipc_receive, ipc_send, IpcMessage, IPC_MSG_REQUEST};
use driver_framework::syscalls;

// VFS Service IPC constants
const VFS_SERVICE_PORT: u32 = 102; // Assuming VFS service listens on port 102
const VFS_MSG_REGISTER_FS: u32 = 1; // Message ID for registering a filesystem

// Message types for VFS operations (simplified)
const FS_OP_MOUNT: u32 = 1;
const FS_OP_UNMOUNT: u32 = 2;
const FS_OP_OPEN: u32 = 3;
const FS_OP_CLOSE: u32 = 4;
const FS_OP_READ: u32 = 5;
const FS_OP_WRITE: u32 = 6;

// Local FAT32 driver port
const FAT32_DRIVER_PORT: u32 = 103; // Arbitrary port for this driver

#[no_mangle]
pub extern "C" fn _start() -> ! {
    let mut fat32_driver_port: u64 = 0;

    // Create IPC port for this FAT32 driver
    if let Ok(port) = ipc_create_port() {
        fat32_driver_port = port;
    } else {
        // Failed to create port, panic or loop
        loop {}
    }

    // Register with VFS service
    let mut register_msg = IpcMessage::new();
    register_msg.msg_type = IPC_MSG_REQUEST;
    register_msg.msg_id = VFS_MSG_REGISTER_FS;
    
    // Inline data should contain filesystem name ("fat32") and this driver's port
    let fs_name = b"fat32\0";
    register_msg.inline_data[0..fs_name.len()].copy_from_slice(fs_name);
    register_msg.inline_data[fs_name.len()] = 0; // Null terminator
    register_msg.inline_data[60..68].copy_from_slice(&fat32_driver_port.to_le_bytes()); // Store our port
    register_msg.inline_size = (fs_name.len() + 1 + 8) as u32; // Name length + null + port_id

    // Send registration request to VFS service
    if ipc_send(VFS_SERVICE_PORT as u64, &register_msg).is_err() {
        // Failed to register, panic or loop
        loop {}
    }

    // Main service loop
    let mut msg = IpcMessage::new();
    loop {
        // Handle filesystem operations via IPC
        if ipc_receive(fat32_driver_port, &mut msg).is_ok() {
            let response = handle_ipc_message(&msg);
            let _ = ipc_send(msg.sender_tid, &response);
        }
        syscalls::sys_sleep(10); // Yield
    }
}

fn handle_ipc_message(msg: &IpcMessage) -> IpcMessage {
    let mut response = IpcMessage::new();
    response.msg_type = driver_framework::ipc::IPC_MSG_RESPONSE;
    response.msg_id = msg.msg_id;

    match msg.msg_id {
        FS_OP_MOUNT => {
            // Placeholder: Mount operation
            response.inline_data[0] = 0; // Success
            response.inline_size = 1;
        }
        FS_OP_UNMOUNT => {
            // Placeholder: Unmount operation
            response.inline_data[0] = 0; // Success
            response.inline_size = 1;
        }
        FS_OP_OPEN => {
            // Placeholder: Open operation
            response.inline_data[0] = 0; // Success
            response.inline_size = 1;
        }
        FS_OP_CLOSE => {
            // Placeholder: Close operation
            response.inline_data[0] = 0; // Success
            response.inline_size = 1;
        }
        FS_OP_READ => {
            // Placeholder: Read operation
            response.inline_data[0] = 0; // Success
            response.inline_size = 1;
        }
        FS_OP_WRITE => {
            // Placeholder: Write operation
            response.inline_data[0] = 0; // Success
            response.inline_size = 1;
        }
        _ => {
            response.inline_data[0] = 0xFF; // Unknown operation
            response.inline_size = 1;
        }
    }
    response
}


#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}