//! AHCI Storage Driver
//! 
//! User-space AHCI driver for SATA storage devices.

#![no_std]
#![no_main]

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

#[no_mangle]
pub extern "C" fn _start() -> ! {
    ahci_driver_init();
    ahci_driver_loop();
}

fn ahci_driver_init() {
    // TODO: Detect AHCI controller
    // TODO: Initialize AHCI ports
    // TODO: Register with device manager
}

fn ahci_driver_loop() {
    loop {
        // TODO: Handle storage I/O requests via IPC
        // TODO: Process command queue
    }
}

