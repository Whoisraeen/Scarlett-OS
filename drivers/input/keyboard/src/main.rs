//! Keyboard Driver
//! 
//! User-space keyboard driver (PS/2)

#![no_std]
#![no_main]

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

// Syscall wrappers
extern "C" {
    fn sys_ipc_send(tid: u32, msg: *const IpcMessage) -> i32;
    fn sys_ipc_register_port(port: u32) -> i32;
    fn sys_io_read(port: u16, size: u8) -> u32;
    fn sys_io_write(port: u16, value: u32, size: u8) -> i32;
    fn sys_irq_register(irq: u32) -> i32;
    fn sys_irq_wait(irq: u32) -> i32;
}

#[repr(C)]
struct IpcMessage {
    sender_tid: u32,
    msg_type: u32,
    data: [u8; 256],
}

const KEYBOARD_PORT: u32 = 201;
const INPUT_SERVER_PORT: u32 = 200; // Assuming Input Server/Compositor is listening here
const DRIVER_MANAGER_PORT: u32 = 100;

const PS2_DATA: u16 = 0x60;
const PS2_CMD: u16 = 0x64;
const PS2_STATUS: u16 = 0x64;

const MSG_KEY_EVENT: u32 = 10;

#[no_mangle]
pub extern "C" fn _start() -> ! {
    keyboard_driver_init();
    keyboard_driver_loop();
}

fn keyboard_driver_init() {
    unsafe {
        // Register IPC port
        sys_ipc_register_port(KEYBOARD_PORT);

        // Register with Driver Manager
        let mut msg = IpcMessage {
            sender_tid: 0,
            msg_type: 1, // REGISTER
            data: [0; 256],
        };
        msg.data[0] = 3; // DRIVER_TYPE_INPUT
        sys_ipc_send(DRIVER_MANAGER_PORT, &msg);

        // Register IRQ 1
        sys_irq_register(1);

        // Initialize PS/2 (Minimal)
        // Disable devices
        sys_io_write(PS2_CMD, 0xAD, 1); // Disable Keyboard
        sys_io_write(PS2_CMD, 0xA7, 1); // Disable Mouse

        // Flush output buffer
        while (sys_io_read(PS2_STATUS, 1) & 1) != 0 {
            sys_io_read(PS2_DATA, 1);
        }

        // Config
        sys_io_write(PS2_CMD, 0x20, 1); // Read Config
        let mut config = sys_io_read(PS2_DATA, 1);
        config |= 1; // Enable Keyboard IRQ
        config &= !0x10; // Enable Keyboard Port
        sys_io_write(PS2_CMD, 0x60, 1); // Write Config
        sys_io_write(PS2_DATA, config, 1);

        // Enable Keyboard
        sys_io_write(PS2_CMD, 0xAE, 1);
    }
}

fn keyboard_driver_loop() -> ! {
    loop {
        unsafe {
            // Wait for IRQ 1
            sys_irq_wait(1);

            // Read Scancode
            let status = sys_io_read(PS2_STATUS, 1);
            if (status & 1) != 0 {
                let scancode = sys_io_read(PS2_DATA, 1) as u8;
                
                // Send to Input Server
                let mut msg = IpcMessage {
                    sender_tid: 0,
                    msg_type: MSG_KEY_EVENT,
                    data: [0; 256],
                };
                msg.data[0] = scancode;
                
                sys_ipc_send(INPUT_SERVER_PORT, &msg);
            }
        }
    }
}