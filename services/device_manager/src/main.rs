//! Device Manager Service
//! 
//! This service runs in user-space and manages device enumeration,
//! driver loading, and device resource allocation.

#![no_std]
#![no_main]

mod ipc;
mod lib;

use core::panic::PanicInfo;
use lib::{init_ipc, handle_enumerate_devices, handle_load_driver, handle_get_device, get_service_port};
use ipc::{IpcMessage, sys_ipc_receive, sys_ipc_send};

/// Panic handler for the device manager service
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    // In a real implementation, we'd log the panic and notify the kernel
    loop {}
}

/// Entry point for the device manager service
#[no_mangle]
pub extern "C" fn _start() -> ! {
    // Initialize device manager
    device_manager_init();
    
    // Main service loop
    device_manager_loop();
}

/// Initialize the device manager service
fn device_manager_init() {
    // Initialize IPC
    if let Ok(port) = init_ipc() {
        let _ = port;
    }
    
    // Initialize device manager
    let _ = lib::init();
}

/// Main service loop - handles IPC messages
fn device_manager_loop() {
    let mut msg = IpcMessage::new();
    let service_port = unsafe {
        // Get service port (should be set during init)
        get_service_port()
    };
    
    loop {
        // Receive IPC message on service port
        if sys_ipc_receive(service_port, &mut msg) == 0 {
            let mut response = match msg.msg_type {
                ipc::IPC_MSG_REQUEST => {
                    // Handle request based on msg_id
                    match msg.msg_id {
                        lib::DEV_MGR_OP_ENUMERATE => handle_enumerate_devices(&msg),
                        lib::DEV_MGR_OP_LOAD_DRIVER => handle_load_driver(&msg),
                        lib::DEV_MGR_OP_GET_DEVICE => handle_get_device(&msg),
                        lib::DEV_MGR_OP_FIND_DEVICE => {
                            // Find device by vendor/device ID or class
                            let mut resp = IpcMessage::new();
                            resp.msg_type = ipc::IPC_MSG_RESPONSE;
                            resp.msg_id = msg.msg_id;
                            
                            if msg.inline_size >= 4 {
                                let vendor_id = u16::from_le_bytes([
                                    msg.inline_data[0],
                                    msg.inline_data[1],
                                ]);
                                let device_id = u16::from_le_bytes([
                                    msg.inline_data[2],
                                    msg.inline_data[3],
                                ]);
                                
                                if let Some(device) = lib::device::find_device_by_pci_id(vendor_id, device_id) {
                                    let device_bytes = unsafe {
                                        core::slice::from_raw_parts(
                                            device as *const _ as *const u8,
                                            core::mem::size_of::<lib::device::Device>()
                                        )
                                    };
                                    let copy_len = device_bytes.len().min(64);
                                    resp.inline_data[0..copy_len].copy_from_slice(&device_bytes[0..copy_len]);
                                    resp.inline_size = copy_len as u32;
                                }
                            }
                            resp
                        },
                        _ => {
                            // Unknown request
                            let mut resp = IpcMessage::new();
                            resp.msg_type = ipc::IPC_MSG_RESPONSE;
                            resp.msg_id = msg.msg_id;
                            resp.inline_data[0] = 0xFF;  // Error code
                            resp.inline_size = 1;
                            resp
                        }
                    }
                },
                _ => {
                    // Unknown message type
                    let mut resp = IpcMessage::new();
                    resp.msg_type = ipc::IPC_MSG_RESPONSE;
                    resp.msg_id = msg.msg_id;
                    resp
                }
            };
            
            // Send response back to sender using sender_tid as reply port.
            let _ = sys_ipc_send(msg.sender_tid, &response);
        }
        
        // Yield to scheduler (if syscall exists)
        // sys_yield();
    }
}

