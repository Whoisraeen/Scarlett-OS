//! IPC communication for drivers

use crate::syscalls;

/// IPC message types
pub const IPC_MSG_DATA: u32 = 0;
pub const IPC_MSG_REQUEST: u32 = 1;
pub const IPC_MSG_RESPONSE: u32 = 2;
pub const IPC_MSG_NOTIFICATION: u32 = 3;


/// IPC message structure (must match kernel/include/ipc/ipc.h)
#[repr(C)]
pub struct IpcMessage {
    pub sender_tid: u64,
    pub msg_id: u64,
    pub msg_type: u32,
    pub inline_size: u32,
    pub inline_data: [u8; 64],
    pub buffer: *mut u8,
    pub buffer_size: usize,
}

impl IpcMessage {
    pub fn new() -> Self {
        Self {
            sender_tid: 0,
            msg_id: 0,
            msg_type: IPC_MSG_REQUEST,
            inline_size: 0,
            inline_data: [0; 64],
            buffer: core::ptr::null_mut(),
            buffer_size: 0,
        }
    }
    
    pub fn set_inline_data(&mut self, data: &[u8]) {
        let len = data.len().min(64);
        self.inline_data[..len].copy_from_slice(&data[..len]);
        self.inline_size = len as u32;
    }
    
    pub fn get_inline_data(&self) -> &[u8] {
        &self.inline_data[..self.inline_size as usize]
    }
}

/// Send IPC message
pub fn ipc_send(port_id: u64, msg: &IpcMessage) -> Result<(), u64> {
    let result = syscalls::ipc_send(port_id, msg as *const IpcMessage as u64);
    if result == 0 {
        Ok(())
    } else {
        Err(result)
    }
}

/// Receive IPC message
pub fn ipc_receive(port_id: u64, msg: &mut IpcMessage) -> Result<(), u64> {
    let result = syscalls::ipc_receive(port_id, msg as *mut IpcMessage as u64);
    if result == 0 {
        Ok(())
    } else {
        Err(result)
    }
}

/// Create IPC port
pub fn ipc_create_port() -> Result<u64, ()> {
    let port_id = syscalls::ipc_create_port();
    if port_id == 0 {
        Err(())
    } else {
        Ok(port_id)
    }
}

