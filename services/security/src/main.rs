#![no_std]
#![no_main]

//! Security Service
//!
//! Manages capabilities, ACLs, and application sandboxing

mod capability;
mod acl;
mod sandbox;

use capability::CapabilityManager;
use sandbox::SandboxManager;

static mut CAP_MANAGER: Option<CapabilityManager> = None;
static mut SANDBOX_MANAGER: Option<SandboxManager> = None;

#[no_mangle]
pub extern "C" fn _start() -> ! {
    unsafe {
        // Initialize managers
        CAP_MANAGER = Some(CapabilityManager::new());
        SANDBOX_MANAGER = Some(SandboxManager::new());

        // Main service loop
        loop {
            // TODO: Handle IPC requests
            // - Capability management
            // - ACL enforcement
            // - Sandbox creation/management
        }
    }
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
