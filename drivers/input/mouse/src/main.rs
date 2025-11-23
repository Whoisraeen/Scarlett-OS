//! Mouse Driver
//! 
//! User-space mouse driver (PS/2)

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

const MOUSE_PORT: u32 = 202;
const INPUT_SERVER_PORT: u32 = 200; 
const DRIVER_MANAGER_PORT: u32 = 100;

const PS2_DATA: u16 = 0x60;
const PS2_CMD: u16 = 0x64;
const PS2_STATUS: u16 = 0x64;

const MSG_MOUSE_EVENT: u32 = 11;

#[no_mangle]
pub extern "C" fn _start() -> ! {
    mouse_driver_init();
    mouse_driver_loop();
}

fn mouse_wait(write: bool) {
    unsafe {
        let timeout = 100000;
        for _ in 0..timeout {
            let status = sys_io_read(PS2_STATUS, 1);
            if write {
                if (status & 2) == 0 { return; }
            } else {
                if (status & 1) == 1 { return; }
            }
        }
    }
}

fn mouse_write(byte: u8) {
    unsafe {
        mouse_wait(true);
        sys_io_write(PS2_CMD, 0xD4, 1); // Write to Auxiliary Device
        mouse_wait(true);
        sys_io_write(PS2_DATA, byte as u32, 1);
    }
}

fn mouse_read() -> u8 {
    unsafe {
        mouse_wait(false);
        sys_io_read(PS2_DATA, 1) as u8
    }
}

fn mouse_driver_init() {
    unsafe {
        sys_ipc_register_port(MOUSE_PORT);

        // Register with Driver Manager
        let mut msg = IpcMessage {
            sender_tid: 0,
            msg_type: 1, // REGISTER
            data: [0; 256],
        };
        msg.data[0] = 3; // DRIVER_TYPE_INPUT
        sys_ipc_send(DRIVER_MANAGER_PORT, &msg);

        // Register IRQ 12
        sys_irq_register(12);

        // Enable Mouse Port
        mouse_wait(true);
        sys_io_write(PS2_CMD, 0xA8, 1);

        // Enable Interrupts
        mouse_wait(true);
        sys_io_write(PS2_CMD, 0x20, 1);
        let mut status = sys_io_read(PS2_DATA, 1);
        status |= 2; // Enable IRQ 12
        mouse_wait(true);
        sys_io_write(PS2_CMD, 0x60, 1);
        mouse_wait(true);
        sys_io_write(PS2_DATA, status, 1);

        // Reset Mouse
        mouse_write(0xFF);
        mouse_read(); // ACK
        mouse_read(); // AA (Success)
        mouse_read(); // Device ID

        // Enable Streaming
        mouse_write(0xF4);
        mouse_read(); // ACK
    }
}

fn mouse_driver_loop() -> ! {
    let mut packet = [0u8; 3];
    let mut packet_idx = 0;

    loop {
        unsafe {
            sys_irq_wait(12);
            let status = sys_io_read(PS2_STATUS, 1);
            if (status & 0x21) == 0x21 { // Data available + Aux data
                let byte = sys_io_read(PS2_DATA, 1) as u8;
                
                packet[packet_idx] = byte;
                packet_idx += 1;

                if packet_idx == 3 {
                    packet_idx = 0;
                    
                    let flags = packet[0];
                    let x = packet[1] as i8; // Relative movement
                    let y = packet[2] as i8;

                    // Send event
                    let mut msg = IpcMessage {
                        sender_tid: 0,
                        msg_type: MSG_MOUSE_EVENT,
                        data: [0; 256],
                    };
                    msg.data[0] = flags;
                    msg.data[1] = x as u8;
                    msg.data[2] = y as u8;
                    
                    sys_ipc_send(INPUT_SERVER_PORT, &msg);
                }
            }
        }
    }
}