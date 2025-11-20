//! Device Manager Service Library

pub mod pci;
pub mod device;
pub mod driver;
pub mod service_registry;
pub mod process_spawn;

pub use crate::ipc::{IpcMessage, IPC_MSG_REQUEST, IPC_MSG_RESPONSE};
pub use pci::{pci_enumerate, pci_get_device_count, pci_get_device, PciDevice};
pub use device::{register_pci_device, get_device, get_device_count, 
                 find_device_by_pci_id, set_device_driver, set_device_state, Device};
pub use driver::{find_driver, load_driver, auto_load_drivers};
pub use service_registry::{ServiceType, register_service_port, notify_service, get_driver_port};

/// Device manager operation types
pub const DEV_MGR_OP_ENUMERATE: u64 = 1;
pub const DEV_MGR_OP_LOAD_DRIVER: u64 = 2;
pub const DEV_MGR_OP_GET_DEVICE: u64 = 3;
pub const DEV_MGR_OP_FIND_DEVICE: u64 = 4;

/// Device manager service port
static mut SERVICE_PORT: u64 = 0;
static mut INITIALIZED: bool = false;

/// Get service port (for internal use)
pub unsafe fn get_service_port() -> u64 {
    SERVICE_PORT
}

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
                // Register all PCI devices
                for i in 0..count {
                    if let Some(pci_dev) = pci_get_device(i) {
                        let _ = device::register_pci_device(pci_dev);
                    }
                }
                
                // Auto-load drivers for all devices
                driver::auto_load_drivers();
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
    
    // Return registered device count (from device registry)
    let count = device::get_device_count() as u32;
    response.inline_data[0..4].copy_from_slice(&count.to_le_bytes());
    response.inline_size = 4;
    
    response
}

/// Handle get device request
pub fn handle_get_device(request: &IpcMessage) -> IpcMessage {
    let mut response = IpcMessage::new();
    response.msg_type = IPC_MSG_RESPONSE;
    response.msg_id = request.msg_id;
    
    // Parse device ID from request
    if request.inline_size >= 4 {
        let device_id = u32::from_le_bytes([
            request.inline_data[0],
            request.inline_data[1],
            request.inline_data[2],
            request.inline_data[3],
        ]);
        
        if let Some(device) = device::get_device(device_id) {
            // Copy device info to response
            let device_bytes = unsafe {
                core::slice::from_raw_parts(
                    device as *const _ as *const u8,
                    core::mem::size_of::<device::Device>()
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
    
    // Parse device ID and driver name from request
    if request.inline_size >= 4 {
        let device_id = u32::from_le_bytes([
            request.inline_data[0],
            request.inline_data[1],
            request.inline_data[2],
            request.inline_data[3],
        ]);
        
        // Extract driver name (null-terminated string starting at offset 4)
        let mut driver_name_bytes = [0u8; 32];
        let name_len = (request.inline_size as usize - 4).min(31);
        for i in 0..name_len {
            driver_name_bytes[i] = request.inline_data[4 + i];
            if driver_name_bytes[i] == 0 {
                break;
            }
        }
        
        // Convert to string slice
        let driver_name = core::str::from_utf8(&driver_name_bytes[..name_len])
            .unwrap_or("unknown");
        
        // Set device driver
        match device::set_device_driver(device_id, driver_name) {
            Ok(_) => {
                // Set device state to initialized
                let _ = device::set_device_state(device_id, device::DeviceState::Initialized);
                response.inline_data[0] = 0;  // Success
            }
            Err(_) => {
                response.inline_data[0] = 1;  // Error
            }
        }
        response.inline_size = 1;
    } else {
        response.inline_data[0] = 2;  // Invalid request
        response.inline_size = 1;
    }
    
    response
}
