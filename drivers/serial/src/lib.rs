#![no_std]


use core::panic::PanicInfo;

// Serial port constants
const COM1: u16 = 0x3F8;

// Helper to write to port
unsafe fn outb(port: u16, value: u8) {
    core::arch::asm!("out dx, al", in("dx") port, in("al") value, options(nomem, nostack));
}

// Helper to read from port
unsafe fn inb(port: u16) -> u8 {
    let value: u8;
    core::arch::asm!("in al, dx", out("al") value, in("dx") port, options(nomem, nostack));
    value
}

#[no_mangle]
pub extern "C" fn rust_serial_init() {
    unsafe {
        outb(COM1 + 1, 0x00);    // Disable all interrupts
        outb(COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
        outb(COM1 + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
        outb(COM1 + 1, 0x00);    //                  (hi byte)
        outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
        outb(COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
        outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    }
}

#[no_mangle]
pub extern "C" fn rust_serial_write(c: u8) {
    unsafe {
        while (inb(COM1 + 5) & 0x20) == 0 {
            // Wait for transmit buffer to be empty
            core::hint::spin_loop();
        }
        outb(COM1, c);
    }
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}
