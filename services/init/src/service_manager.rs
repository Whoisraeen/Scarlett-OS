//! Service Manager for Init Service

use core::mem;

/// Service information
#[repr(C)]
pub struct ServiceInfo {
    pub name: [u8; 32],
    pub pid: u64,
    pub port: u64,
    pub status: ServiceStatus,
}

#[repr(u32)]
pub enum ServiceStatus {
    Stopped = 0,
    Starting = 1,
    Running = 2,
    Stopping = 3,
    Failed = 4,
}

const MAX_SERVICES: usize = 16;
static mut SERVICES: [ServiceInfo; MAX_SERVICES] = unsafe { mem::zeroed() };
static mut SERVICE_COUNT: usize = 0;

/// Service names
pub const SERVICE_DEVICE_MANAGER: &[u8] = b"device_manager\0";
pub const SERVICE_VFS: &[u8] = b"vfs\0";
pub const SERVICE_NETWORK: &[u8] = b"network\0";
pub const SERVICE_COMPOSITOR: &[u8] = b"compositor\0";
pub const SERVICE_WINDOW_MANAGER: &[u8] = b"window_manager\0";

/// Register a service
pub fn register_service(name: &[u8], pid: u64, port: u64) -> Result<usize, ()> {
    unsafe {
        if SERVICE_COUNT >= MAX_SERVICES {
            return Err(());
        }
        
        let service = &mut SERVICES[SERVICE_COUNT];
        
        // Copy name
        let name_len = name.len().min(31);
        service.name[0..name_len].copy_from_slice(&name[0..name_len]);
        service.name[name_len] = 0;
        
        service.pid = pid;
        service.port = port;
        service.status = ServiceStatus::Running;
        
        let idx = SERVICE_COUNT;
        SERVICE_COUNT += 1;
        
        Ok(idx)
    }
}

/// Find service by name
pub fn find_service(name: &[u8]) -> Option<usize> {
    unsafe {
        for i in 0..SERVICE_COUNT {
            let service = &SERVICES[i];
            if service.name.starts_with(name) && service.name[name.len()] == 0 {
                return Some(i);
            }
        }
        None
    }
}

/// Get service port
pub fn get_service_port(service_idx: usize) -> Option<u64> {
    unsafe {
        if service_idx < SERVICE_COUNT {
            Some(SERVICES[service_idx].port)
        } else {
            None
        }
    }
}

/// Set service status
pub fn set_service_status(service_idx: usize, status: ServiceStatus) {
    unsafe {
        if service_idx < SERVICE_COUNT {
            SERVICES[service_idx].status = status;
        }
    }
}

