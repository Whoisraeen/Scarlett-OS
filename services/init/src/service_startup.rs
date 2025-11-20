//! Service startup and management

use crate::service_manager::{register_service, find_service, get_service_port, set_service_status, ServiceStatus};
use core::mem;

/// Service startup configuration
pub struct ServiceConfig {
    pub name: &'static [u8],
    pub binary_path: &'static [u8],
    pub dependencies: &'static [&'static [u8]],
}

/// Core services to start
pub const CORE_SERVICES: &[ServiceConfig] = &[
    ServiceConfig {
        name: b"device_manager\0",
        binary_path: b"/services/device_manager\0",
        dependencies: &[],
    },
    ServiceConfig {
        name: b"vfs\0",
        binary_path: b"/services/vfs\0",
        dependencies: &[b"device_manager\0"],
    },
    ServiceConfig {
        name: b"network\0",
        binary_path: b"/services/network\0",
        dependencies: &[b"device_manager\0"],
    },
];

/// Start a service
pub fn start_service(config: &ServiceConfig) -> Result<u64, ()> {
    // Check dependencies
    for dep in config.dependencies {
        if find_service(dep).is_none() {
            // Dependency not ready
            return Err(());
        }
    }
    
    // TODO: Use SYS_EXEC to start service binary
    // For now, services will start themselves and register
    
    // Mark service as starting
    if let Some(idx) = find_service(config.name) {
        set_service_status(idx, ServiceStatus::Starting);
    }
    
    // Return placeholder PID (will be actual PID when exec is implemented)
    Ok(1)
}

/// Start all core services
pub fn start_core_services() {
    for service in CORE_SERVICES {
        // Try to start service
        if start_service(service).is_ok() {
            // Service started
        } else {
            // Service failed to start (dependency not ready, etc.)
        }
    }
}

/// Wait for service to be ready
pub fn wait_for_service(name: &[u8], timeout_ms: u64) -> bool {
    // TODO: Implement timeout
    let _ = timeout_ms;
    
    // Check if service is registered
    find_service(name).is_some()
}

/// Wait for all core services
pub fn wait_for_core_services() {
    for service in CORE_SERVICES {
        // Wait for service to register
        while !wait_for_service(service.name, 5000) {
            yield_cpu();
        }
    }
}

/// Yield CPU to scheduler
fn yield_cpu() {
    unsafe {
        syscall_raw(6, 0, 0, 0, 0, 0);  // SYS_YIELD
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

