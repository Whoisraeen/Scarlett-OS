//! FAT32 Filesystem Driver
//! 
//! User-space FAT32 filesystem driver

#![no_std]
#![no_main]

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

#[no_mangle]
pub extern "C" fn _start() -> ! {
    // FAT32 driver main loop
    // This driver will be loaded by the VFS service when needed
    loop {
        // TODO: Handle filesystem operations via IPC
        // TODO: Register with VFS service
    }
}

