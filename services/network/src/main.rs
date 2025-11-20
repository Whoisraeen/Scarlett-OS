//! Network Service
//! 
//! User-space network stack service that provides TCP/IP networking
//! via IPC to other processes.

#![no_std]
#![no_main]

mod network;
mod ipc;

use core::panic::PanicInfo;
use network::network_init;
use ipc::{IpcMessage, sys_ipc_receive};

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

#[no_mangle]
pub extern "C" fn _start() -> ! {
    // Initialize network stack
    let _ = network_init();
    
    // Main service loop
    network_loop();
}

fn network_loop() {
    let mut msg = IpcMessage::new();
    
    loop {
        // Receive IPC messages for network operations
        if sys_ipc_receive(3, &mut msg) == 0 {
            // TODO: Handle socket creation requests
            // TODO: Handle connect, bind, listen, accept requests
            // TODO: Handle send, receive requests
            // TODO: Process network packets from drivers
        }
    }
}

