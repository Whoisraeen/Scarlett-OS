//! IPC interface for driver manager

pub const IPC_MSG_REQUEST: u32 = 1;
pub const IPC_MSG_RESPONSE: u32 = 2;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct IpcMessage {
    pub msg_type: u32,
    pub msg_id: u32,
    pub sender_tid: u32,
    pub inline_size: u32,
    pub inline_data: [u8; 128],
}

impl IpcMessage {
    pub fn new() -> Self {
        IpcMessage {
            msg_type: 0,
            msg_id: 0,
            sender_tid: 0,
            inline_size: 0,
            inline_data: [0; 128],
        }
    }
}

// Syscall wrappers
extern "C" {
    fn syscall_ipc_send(port: u32, msg: *const IpcMessage) -> i32;
    fn syscall_ipc_receive(port: u32, msg: *mut IpcMessage) -> i32;
    fn syscall_ipc_register_port(port: u32) -> i32;
}

pub fn sys_ipc_send(port: u32, msg: &IpcMessage) -> i32 {
    unsafe { syscall_ipc_send(port, msg as *const IpcMessage) }
}

pub fn sys_ipc_receive(port: u32, msg: &mut IpcMessage) -> i32 {
    unsafe { syscall_ipc_receive(port, msg as *mut IpcMessage) }
}

pub fn sys_ipc_register_port(port: u32) -> i32 {
    unsafe { syscall_ipc_register_port(port) }
}
