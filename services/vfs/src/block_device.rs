//! Block device communication for VFS service

use crate::ipc::{IpcMessage, ipc_send, ipc_receive, sys_ipc_send, sys_ipc_receive};

/// Block device service port (AHCI driver)
static mut BLOCK_DEV_PORT: u64 = 0;

/// Set block device port
pub fn set_block_device_port(port: u64) {
    unsafe {
        BLOCK_DEV_PORT = port;
    }
}

/// Read blocks from block device
/// Returns data in response buffer (caller must provide buffer)
pub fn read_blocks(port_idx: u8, lba: u64, count: u32, buffer: &mut [u8]) -> Result<usize, ()> {
    unsafe {
        if BLOCK_DEV_PORT == 0 {
            return Err(()); // Driver not available
        }
        
        let mut request = IpcMessage::new();
        request.msg_id = 1; // BLOCK_DEV_OP_READ
        request.msg_type = crate::ipc::IPC_MSG_REQUEST;
        
        // Pack request: port_idx, lba, count
        request.inline_data[0] = port_idx;
        request.inline_data[1..9].copy_from_slice(&lba.to_le_bytes());
        request.inline_data[9..13].copy_from_slice(&count.to_le_bytes());
        request.inline_size = 13;
        
        // Send request with retry logic
        let mut retries = 3;
        loop {
            match ipc_send(BLOCK_DEV_PORT, &request) {
                Ok(_) => break,
                Err(_) => {
                    retries -= 1;
                    if retries == 0 {
                        return Err(()); // Failed after retries
                    }
                    crate::syscalls::sys_yield();
                }
            }
        }
        
        // Receive response with retry logic
        let mut response = IpcMessage::new();
        retries = 3;
        loop {
            match ipc_receive(BLOCK_DEV_PORT, &mut response) {
                Ok(_) => break,
                Err(_) => {
                    retries -= 1;
                    if retries == 0 {
                        return Err(()); // Failed after retries
                    }
                    crate::syscalls::sys_yield();
                }
            }
        }
        
        // Extract data from response
        // Note: For large reads, data would be in response.buffer
        let copy_len = buffer.len().min(response.inline_size as usize);
        buffer[0..copy_len].copy_from_slice(&response.inline_data[0..copy_len]);
        
        Ok(copy_len)
    }
}

/// Write blocks to block device
pub fn write_blocks(port_idx: u8, lba: u64, count: u32, data: &[u8]) -> Result<(), ()> {
    unsafe {
        if BLOCK_DEV_PORT == 0 {
            return Err(());
        }
        
        let mut request = IpcMessage::new();
        request.msg_id = 2; // BLOCK_DEV_OP_WRITE
        request.msg_type = crate::ipc::IPC_MSG_REQUEST;
        
        // Pack request: port_idx, lba, count
        request.inline_data[0] = port_idx;
        request.inline_data[1..9].copy_from_slice(&lba.to_le_bytes());
        request.inline_data[9..13].copy_from_slice(&count.to_le_bytes());
        request.inline_size = 13;
        
        // For large writes, data would be in request.buffer
        // For now, copy what fits in inline_data
        let copy_len = data.len().min(64 - 13);
        request.inline_data[13..13+copy_len].copy_from_slice(&data[0..copy_len]);
        request.inline_size = (13 + copy_len) as u32;
        
        // Send request with retry logic
        let mut retries = 3;
        loop {
            match ipc_send(BLOCK_DEV_PORT, &request) {
                Ok(_) => break,
                Err(_) => {
                    retries -= 1;
                    if retries == 0 {
                        return Err(()); // Failed after retries
                    }
                    crate::syscalls::sys_yield();
                }
            }
        }
        
        // Receive response with retry logic
        let mut response = IpcMessage::new();
        retries = 3;
        loop {
            match ipc_receive(BLOCK_DEV_PORT, &mut response) {
                Ok(_) => break,
                Err(_) => {
                    retries -= 1;
                    if retries == 0 {
                        return Err(()); // Failed after retries
                    }
                    crate::syscalls::sys_yield();
                }
            }
        }
        
        // Check success
        if response.inline_size > 0 && response.inline_data[0] == 0 {
            Ok(())
        } else {
            Err(())
        }
    }
}

