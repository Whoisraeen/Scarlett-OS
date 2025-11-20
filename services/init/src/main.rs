//! Init Service
//! 
//! This is the first user-space process started by the kernel.
//! It initializes system services and launches the desktop environment.

#![no_std]
#![no_main]

mod service_manager;
mod service_startup;

use core::panic::PanicInfo;
use service_manager::{register_service, SERVICE_DEVICE_MANAGER, SERVICE_VFS, SERVICE_NETWORK};
use service_startup::{start_core_services, wait_for_core_services};

/// Panic handler for init
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

/// Entry point for init service
#[no_mangle]
pub extern "C" fn _start() -> ! {
    init_main();
}

/// Main initialization function
fn init_main() {
    // Start core services
    start_core_services();
    
    // Wait for services to register
    wait_for_core_services();
    
    // Keep running - monitor services
    loop {
        // TODO: Monitor child processes
        // TODO: Handle service restarts
        // TODO: Handle service failures
        // TODO: Check service health
        yield_cpu();
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
