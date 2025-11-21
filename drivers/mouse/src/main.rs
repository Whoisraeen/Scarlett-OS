/**
 * @file mouse_driver.rs
 * @brief User-space PS/2 mouse driver
 */

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

// IPC syscall wrappers
extern "C" {
    fn sys_ipc_send(tid: u32, msg: *const IpcMessage) -> i32;
    fn sys_ipc_receive(port: u32, msg: *mut IpcMessage) -> i32;
    fn sys_ipc_register_port(port: u32) -> i32;
    fn sys_io_read(port: u16, size: u8) -> u32;
    fn sys_io_write(port: u16, value: u32, size: u8) -> i32;
    fn sys_irq_register(irq: u32) -> i32;
    fn sys_irq_wait() -> u32;
}

#[repr(C)]
struct IpcMessage {
    sender_tid: u32,
    msg_type: u32,
    data: [u8; 256],
}

// Mouse IPC port
const MOUSE_DRIVER_PORT: u32 = 104;

// PS/2 mouse ports
const MOUSE_DATA_PORT: u16 = 0x60;
const MOUSE_STATUS_PORT: u16 = 0x64;
const MOUSE_COMMAND_PORT: u16 = 0x64;

// Mouse IRQ
const MOUSE_IRQ: u32 = 12;

// Message types
const MSG_MOUSE_GET_EVENT: u32 = 1;
const MSG_MOUSE_SET_RESOLUTION: u32 = 2;

// Mouse state
static mut MOUSE_X: i32 = 0;
static mut MOUSE_Y: i32 = 0;
static mut MOUSE_BUTTONS: u8 = 0;
static mut MOUSE_CYCLE: u8 = 0;
static mut MOUSE_PACKET: [u8; 3] = [0; 3];

#[no_mangle]
pub extern "C" fn _start() -> ! {
    // Register with driver manager
    register_with_driver_manager();

    // Initialize mouse
    init_mouse();

    // Register IRQ handler
    unsafe {
        sys_irq_register(MOUSE_IRQ);
    }

    // Register IPC port
    unsafe {
        sys_ipc_register_port(MOUSE_DRIVER_PORT);
    }

    // Main service loop
    loop {
        // Wait for IRQ
        unsafe {
            let irq = sys_irq_wait();
            if irq == MOUSE_IRQ {
                handle_mouse_interrupt();
            }
        }

        // Check for IPC messages
        let mut msg = IpcMessage {
            sender_tid: 0,
            msg_type: 0,
            data: [0; 256],
        };

        unsafe {
            if sys_ipc_receive(MOUSE_DRIVER_PORT, &mut msg) == 0 {
                let response = handle_message(&msg);
                let _ = sys_ipc_send(msg.sender_tid, &response);
            }
        }
    }
}

fn register_with_driver_manager() {
    // TODO: Send registration message to driver manager
}

fn init_mouse() {
    unsafe {
        // Enable auxiliary device
        mouse_wait(1);
        sys_io_write(MOUSE_COMMAND_PORT, 0xA8, 1);

        // Enable interrupts
        mouse_wait(1);
        sys_io_write(MOUSE_COMMAND_PORT, 0x20, 1);
        mouse_wait(0);
        let mut status = sys_io_read(MOUSE_DATA_PORT, 1) as u8;
        status |= 0x02; // Enable IRQ12
        mouse_wait(1);
        sys_io_write(MOUSE_COMMAND_PORT, 0x60, 1);
        mouse_wait(1);
        sys_io_write(MOUSE_DATA_PORT, status as u32, 1);

        // Use default settings
        mouse_write(0xF6);
        mouse_read();

        // Enable data reporting
        mouse_write(0xF4);
        mouse_read();
    }
}

fn mouse_wait(wait_type: u8) {
    unsafe {
        let timeout = 100000;
        if wait_type == 0 {
            // Wait for output buffer
            for _ in 0..timeout {
                if (sys_io_read(MOUSE_STATUS_PORT, 1) & 0x01) != 0 {
                    return;
                }
            }
        } else {
            // Wait for input buffer
            for _ in 0..timeout {
                if (sys_io_read(MOUSE_STATUS_PORT, 1) & 0x02) == 0 {
                    return;
                }
            }
        }
    }
}

fn mouse_write(value: u8) {
    unsafe {
        mouse_wait(1);
        sys_io_write(MOUSE_COMMAND_PORT, 0xD4, 1);
        mouse_wait(1);
        sys_io_write(MOUSE_DATA_PORT, value as u32, 1);
    }
}

fn mouse_read() -> u8 {
    unsafe {
        mouse_wait(0);
        sys_io_read(MOUSE_DATA_PORT, 1) as u8
    }
}

fn handle_mouse_interrupt() {
    unsafe {
        let data = sys_io_read(MOUSE_DATA_PORT, 1) as u8;

        MOUSE_PACKET[MOUSE_CYCLE as usize] = data;
        MOUSE_CYCLE += 1;

        if MOUSE_CYCLE == 3 {
            MOUSE_CYCLE = 0;

            // Parse packet
            let flags = MOUSE_PACKET[0];
            let dx = MOUSE_PACKET[1] as i8;
            let dy = MOUSE_PACKET[2] as i8;

            // Update position
            MOUSE_X += dx as i32;
            MOUSE_Y -= dy as i32; // Y is inverted

            // Clamp to screen (assuming 1024x768 for now)
            if MOUSE_X < 0 {
                MOUSE_X = 0;
            }
            if MOUSE_X > 1023 {
                MOUSE_X = 1023;
            }
            if MOUSE_Y < 0 {
                MOUSE_Y = 0;
            }
            if MOUSE_Y > 767 {
                MOUSE_Y = 767;
            }

            // Update buttons
            MOUSE_BUTTONS = flags & 0x07;
        }
    }
}

fn handle_message(msg: &IpcMessage) -> IpcMessage {
    match msg.msg_type {
        MSG_MOUSE_GET_EVENT => handle_get_event(),
        MSG_MOUSE_SET_RESOLUTION => handle_set_resolution(msg),
        _ => create_error_response(1),
    }
}

fn handle_get_event() -> IpcMessage {
    unsafe {
        let mut response = IpcMessage {
            sender_tid: 0,
            msg_type: 0,
            data: [0; 256],
        };

        // Pack mouse state into response
        response.data[0..4].copy_from_slice(&MOUSE_X.to_le_bytes());
        response.data[4..8].copy_from_slice(&MOUSE_Y.to_le_bytes());
        response.data[8] = MOUSE_BUTTONS;

        response
    }
}

fn handle_set_resolution(_msg: &IpcMessage) -> IpcMessage {
    // TODO: Implement resolution setting
    create_success_response()
}

fn create_success_response() -> IpcMessage {
    IpcMessage {
        sender_tid: 0,
        msg_type: 0,
        data: [0; 256],
    }
}

fn create_error_response(error_code: u32) -> IpcMessage {
    let mut response = IpcMessage {
        sender_tid: 0,
        msg_type: 1,
        data: [0; 256],
    };
    response.data[0..4].copy_from_slice(&error_code.to_le_bytes());
    response
}
