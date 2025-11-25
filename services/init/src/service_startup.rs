//! Service startup and management

use crate::service_manager::{register_service, find_service, get_service_port, set_service_status, ServiceStatus};
use core::mem;

// Syscall numbers (assuming these are globally available or defined in a common header)
const SYS_FORK: u64 = 15;
const SYS_EXEC: u64 = 16;
const SYS_EXIT: u64 = 0;
const SYS_YIELD: u64 = 6;
const SYS_SLEEP: u64 = 5; // Added for timeout
const SYS_GET_UPTIME_MS: u64 = 47; // Added for timeout

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
    
    // Fork process
    let pid = unsafe { syscall_raw(SYS_FORK, 0, 0, 0, 0, 0) };
    
    if pid == 0 {
        // Child process - Execute service binary
        let path = config.binary_path.as_ptr() as u64;
        
        // Prepare argv (program name, NULL)
        let argv: [*const u8; 2] = [config.binary_path.as_ptr(), core::ptr::null()];
        
        // Prepare envp (NULL)
        let envp: [*const u8; 1] = [core::ptr::null()];
        
        unsafe { 
            syscall_raw(SYS_EXEC, path, argv.as_ptr() as u64, envp.as_ptr() as u64, 0, 0);
            
            // If exec returns, it failed. Exit with error.
            syscall_raw(SYS_EXIT, 1, 0, 0, 0, 0); // SYS_EXIT
        }
        loop {}
    } else if pid > 0xFFFFFFFFFFFFF000 {
        // Fork failed (error code returned as large unsigned)
        return Err(());
    }
    
    // Parent process
    // Mark service as starting
    if let Some(idx) = find_service(config.name) {
        set_service_status(idx, ServiceStatus::Starting);
    }
    
    // Return PID
    Ok(pid)
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
    let start_time = unsafe { syscall_raw(SYS_GET_UPTIME_MS, 0, 0, 0, 0, 0) };
    
    loop {
        if find_service(name).is_some() {
            return true;
        }
        
        let current_time = unsafe { syscall_raw(SYS_GET_UPTIME_MS, 0, 0, 0, 0, 0) };
        if timeout_ms > 0 && current_time - start_time > timeout_ms as u64 {
            return false; // Timeout
        }
        
        unsafe { syscall_raw(SYS_SLEEP, 10, 0, 0, 0, 0) }; // Sleep for a short period
    }
}

/// Wait for all core services
pub fn wait_for_core_services() {
    for service in CORE_SERVICES {
        // Wait for service to register
        while !wait_for_service(service.name, 5000) { // 5-second timeout per service
            // This loop condition already handles yielding/sleeping internally
        }
    }
}

/// Yield CPU to scheduler
fn yield_cpu() {
    unsafe {
        syscall_raw(SYS_YIELD, 0, 0, 0, 0, 0);  // SYS_YIELD
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

