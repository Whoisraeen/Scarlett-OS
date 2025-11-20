//! Mouse Driver
//! 
//! User-space mouse driver that communicates with input devices
//! via the device manager service.

#![no_std]
#![no_main]

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

#[no_mangle]
pub extern "C" fn _start() -> ! {
    mouse_driver_init();
    mouse_driver_loop();
}

fn mouse_driver_init() {
    // TODO: Register with device manager
    // TODO: Initialize PS/2 or USB mouse
    // TODO: Set up interrupt handling
}

fn mouse_driver_loop() {
    loop {
        // TODO: Read mouse input
        // TODO: Send mouse events via IPC to input server
    }
}

