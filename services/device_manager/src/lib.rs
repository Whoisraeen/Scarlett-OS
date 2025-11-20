//! Device Manager Service Library

pub mod pci;

pub use crate::ipc::{IpcMessage, IPC_MSG_REQUEST, IPC_MSG_RESPONSE};
use pci::{pci_enumerate, pci_get_device_count, pci_get_device};

/// Device manager operation types
pub const DEV_MGR_OP_ENUMERATE: u64 = 1;
pub const DEV_MGR_OP_LOAD_DRIVER: u64 = 2;
pub const DEV_MGR_OP_GET_DEVICE: u64 = 3;
pub const DEV_MGR_OP_FIND_DEVICE: u64 = 4;

/// Device manager service port
static mut SERVICE_PORT: u64 = 0;
static mut INITIALIZED: bool = false;

/// Initialize device manager IPC
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

/// Initialize device manager
pub fn init() -> Result<(), ()> {
    unsafe {
        if INITIALIZED {
            return Ok(());
        }
        
        // Enumerate PCI devices
        match pci_enumerate() {
            Ok(count) => {
                // PCI enumeration successful
                let _ = count;
            }
            Err(_) => {
                // Enumeration failed, continue anyway
            }
        }
        
        INITIALIZED = true;
        Ok(())
    }
}

/// Handle device enumeration request
pub fn handle_enumerate_devices(request: &IpcMessage) -> IpcMessage {
    let mut response = IpcMessage::new();
    response.msg_type = IPC_MSG_RESPONSE;
    response.msg_id = request.msg_id;
    
    // Enumerate PCI devices if not already done
    let _ = pci_enumerate();
    
    // Return device count in inline data
    let count = pci_get_device_count() as u32;
    response.inline_data[0..4].copy_from_slice(&count.to_le_bytes());
    response.inline_size = 4;
    
    response
}

/// Handle get device request
pub fn handle_get_device(request: &IpcMessage) -> IpcMessage {
    let mut response = IpcMessage::new();
    response.msg_type = IPC_MSG_RESPONSE;
    response.msg_id = request.msg_id;
    
    // Parse index from request
    if request.inline_size >= 4 {
        let index = u32::from_le_bytes([
            request.inline_data[0],
            request.inline_data[1],
            request.inline_data[2],
            request.inline_data[3],
        ]) as usize;
        
        if let Some(device) = pci_get_device(index) {
            // Copy device info to response
            let device_bytes = unsafe {
                core::slice::from_raw_parts(
                    device as *const _ as *const u8,
                    core::mem::size_of::<pci::PciDevice>()
                )
            };
            let copy_len = device_bytes.len().min(64);
            response.inline_data[0..copy_len].copy_from_slice(&device_bytes[0..copy_len]);
            response.inline_size = copy_len as u32;
        }
    }
    
    response
}

/// Handle driver load request
pub fn handle_load_driver(request: &IpcMessage) -> IpcMessage {
    let mut response = IpcMessage::new();
    response.msg_type = IPC_MSG_RESPONSE;
    response.msg_id = request.msg_id;
    
    // TODO: Parse driver name/path from request
    // TODO: Load driver binary
    // TODO: Initialize driver
    // TODO: Register driver with device
    
    // For now, return success
    response.inline_data[0] = 0;  // Success
    response.inline_size = 1;
    
    response
}
