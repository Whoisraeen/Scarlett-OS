//! ATA Storage Driver
//! 
//! User-space ATA driver for IDE/PATA storage devices.

#![no_std]
#![no_main]

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

#[no_mangle]
pub extern "C" fn _start() -> ! {
    ata_driver_init();
    ata_driver_loop();
}

fn ata_driver_init() {
    // TODO: Detect ATA controller
    // TODO: Initialize ATA ports
    // TODO: Register with device manager
}

fn ata_driver_loop() {
    loop {
        // TODO: Handle storage I/O requests via IPC
        // TODO: Process command queue
    }
}

