//! IPC communication utilities for VFS service

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
}

/// System call wrapper for IPC send
#[no_mangle]
pub extern "C" fn sys_ipc_send(port_id: u64, msg: *const IpcMessage) -> i32 {
    unsafe {
        syscall_raw(9, port_id, msg as u64, 0, 0, 0) as i32
    }
}

/// System call wrapper for IPC receive
#[no_mangle]
pub extern "C" fn sys_ipc_receive(port_id: u64, msg: *mut IpcMessage) -> i32 {
    unsafe {
        syscall_raw(10, port_id, msg as u64, 0, 0, 0) as i32
    }
}

/// Raw syscall function (architecture-specific)
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

