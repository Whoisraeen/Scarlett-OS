//! Service registry for driver-to-service connections

use crate::ipc::{IpcMessage, ipc_send, ipc_receive};

/// Service types
#[derive(Clone, Copy)]
pub enum ServiceType {
    BlockDevice = 1,
    NetworkDevice = 2,
}

/// Service registration
pub struct ServiceRegistration {
    pub service_type: ServiceType,
    pub service_port: u64,
    pub driver_port: u64,
}

/// Service registry
static mut REGISTRY: [Option<ServiceRegistration>; 16] = [None; 16];
static mut REGISTRY_COUNT: usize = 0;

/// Register a service with a driver
pub fn register_service(service_type: ServiceType, service_port: u64, driver_port: u64) -> Result<(), ()> {
    unsafe {
        if REGISTRY_COUNT >= 16 {
            return Err(());
        }
        
        REGISTRY[REGISTRY_COUNT] = Some(ServiceRegistration {
            service_type,
            service_port,
            driver_port,
        });
        REGISTRY_COUNT += 1;
        
        Ok(())
    }
}

/// Get driver port for a service type
pub fn get_driver_port(service_type: ServiceType) -> Option<u64> {
    unsafe {
        for i in 0..REGISTRY_COUNT {
            if let Some(reg) = &REGISTRY[i] {
                if reg.service_type as u8 == service_type as u8 {
                    return Some(reg.driver_port);
                }
            }
        }
        None
    }
}

/// Notify service about new driver
pub fn notify_service(service_type: ServiceType, driver_port: u64) -> Result<(), ()> {
    unsafe {
        for i in 0..REGISTRY_COUNT {
            if let Some(reg) = &REGISTRY[i] {
                if reg.service_type as u8 == service_type as u8 {
                    // Send notification to service
                    let mut msg = IpcMessage::new();
                    msg.msg_type = crate::ipc::IPC_MSG_REQUEST;
                    msg.msg_id = 100; // SERVICE_NOTIFY_DRIVER_AVAILABLE
                    msg.inline_data[0..8].copy_from_slice(&driver_port.to_le_bytes());
                    msg.inline_size = 8;
                    
                    let _ = ipc_send(reg.service_port, &msg);
                    return Ok(());
                }
            }
        }
        Err(())
    }
}

/// Register service port (called by services during init)
pub fn register_service_port(service_type: ServiceType, service_port: u64) -> Result<(), ()> {
    unsafe {
        // Check if driver already exists for this service type
        if let Some(driver_port) = get_driver_port(service_type) {
            // Driver already exists, register connection
            register_service(service_type, service_port, driver_port)
        } else {
            // No driver yet, just register service port
            // Driver will connect later via notify_service
            register_service(service_type, service_port, 0)
        }
    }
}

