//! IPC communication with block device driver

use core::mem;
use driver_framework::ipc::{ipc_send, IpcMessage, IPC_MSG_REQUEST, ipc_receive}; // Need ipc_receive for response
use driver_framework::syscalls; // For sys_get_pid or similar if needed

/// Block device operation types
pub const BLOCK_OP_READ: u64 = 1;
pub const BLOCK_OP_WRITE: u64 = 2;

/// Block device message structure (for IPC inline_data)
#[repr(C, packed)]
pub struct BlockDeviceIpcData {
    pub device_id_u8: u8, // Using u8 for device_id
    pub lba: u64,
    pub count: u32,
}

/// Send block read request
pub fn send_block_read(block_device_driver_port: u64, device_id: u8, lba: u64, count: u32, buffer: &mut [u8]) -> Result<(), ()> {
    let mut msg = IpcMessage::new();
    msg.msg_type = IPC_MSG_REQUEST;
    msg.msg_id = BLOCK_OP_READ;
    
    let ipc_data = BlockDeviceIpcData {
        device_id_u8: device_id,
        lba: lba,
        count: count,
    };
    
    // Copy BlockDeviceIpcData to inline_data
    msg.inline_data[0..mem::size_of::<BlockDeviceIpcData>()].copy_from_slice(unsafe {
        core::slice::from_raw_parts(&ipc_data as *const _ as *const u8, mem::size_of::<BlockDeviceIpcData>())
    });
    msg.inline_size = mem::size_of::<BlockDeviceIpcData>() as u32;

    // Attach buffer for DMA if supported, otherwise rely on inline_data/copy
    msg.buffer = buffer.as_mut_ptr() as u64; // Virtual address
    msg.buffer_size = buffer.len() as u64;
    
    if ipc_send(block_device_driver_port, &msg).is_err() {
        return Err(());
    }

    // Wait for response
    let mut response = IpcMessage::new();
    if ipc_receive(syscalls::sys_get_ipc_port(), &mut response).is_ok() { // Receive on own port
        if response.msg_id == BLOCK_OP_READ && response.msg_type == 2 { // Response type
            return Ok(());
        }
    }
    Err(())
}

/// Send block write request
pub fn send_block_write(block_device_driver_port: u64, device_id: u8, lba: u64, count: u32, buffer: &[u8]) -> Result<(), ()> {
    let mut msg = IpcMessage::new();
    msg.msg_type = IPC_MSG_REQUEST;
    msg.msg_id = BLOCK_OP_WRITE;
    
    let ipc_data = BlockDeviceIpcData {
        device_id_u8: device_id,
        lba: lba,
        count: count,
    };
    
    msg.inline_data[0..mem::size_of::<BlockDeviceIpcData>()].copy_from_slice(unsafe {
        core::slice::from_raw_parts(&ipc_data as *const _ as *const u8, mem::size_of::<BlockDeviceIpcData>())
    });
    msg.inline_size = mem::size_of::<BlockDeviceIpcData>() as u32;

    msg.buffer = buffer.as_ptr() as u64; // Virtual address
    msg.buffer_size = buffer.len() as u64;
    
    if ipc_send(block_device_driver_port, &msg).is_err() {
        return Err(());
    }

    // Wait for response
    let mut response = IpcMessage::new();
    if ipc_receive(syscalls::sys_get_ipc_port(), &mut response).is_ok() { // Receive on own port
        if response.msg_id == BLOCK_OP_WRITE && response.msg_type == 2 { // Response type
            return Ok(());
        }
    }
    Err(())
}

