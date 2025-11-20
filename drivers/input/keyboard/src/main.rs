//! Keyboard Driver
//! 
//! User-space keyboard driver that communicates with input devices
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
    keyboard_driver_init();
    keyboard_driver_loop();
}

fn keyboard_driver_init() {
    // TODO: Register with device manager
    // TODO: Initialize PS/2 or USB keyboard
    // TODO: Set up interrupt handling
}

fn keyboard_driver_loop() {
    loop {
        // TODO: Read keyboard input
        // TODO: Send key events via IPC to input server
    }
}

