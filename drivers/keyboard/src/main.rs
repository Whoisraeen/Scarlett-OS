/**
 * @file keyboard_driver.rs
 * @brief User-space PS/2 keyboard driver
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

// Keyboard IPC port
const KEYBOARD_DRIVER_PORT: u32 = 103;

// PS/2 keyboard ports
const KEYBOARD_DATA_PORT: u16 = 0x60;
const KEYBOARD_STATUS_PORT: u16 = 0x64;
const KEYBOARD_COMMAND_PORT: u16 = 0x64;

// Keyboard IRQ
const KEYBOARD_IRQ: u32 = 1;

// Message types
const MSG_KEYBOARD_GET_KEY: u32 = 1;
const MSG_KEYBOARD_SET_LEDS: u32 = 2;

// Key buffer
const KEY_BUFFER_SIZE: usize = 128;
static mut KEY_BUFFER: [u8; KEY_BUFFER_SIZE] = [0; KEY_BUFFER_SIZE];
static mut KEY_BUFFER_HEAD: usize = 0;
static mut KEY_BUFFER_TAIL: usize = 0;

// US QWERTY scancode to ASCII map
static SCANCODE_TO_ASCII: [u8; 128] = [
    0, 27, b'1', b'2', b'3', b'4', b'5', b'6', b'7', b'8', b'9', b'0', b'-', b'=', 8, // backspace
    b'\t', b'q', b'w', b'e', b'r', b't', b'y', b'u', b'i', b'o', b'p', b'[', b']', b'\n',
    0, // ctrl
    b'a', b's', b'd', b'f', b'g', b'h', b'j', b'k', b'l', b';', b'\'', b'`',
    0, // left shift
    b'\\', b'z', b'x', b'c', b'v', b'b', b'n', b'm', b',', b'.', b'/', 
    0, // right shift
    b'*',
    0, // alt
    b' ', // space
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // F1-F10
    0, // num lock
    0, // scroll lock
    b'7', b'8', b'9', b'-',
    b'4', b'5', b'6', b'+',
    b'1', b'2', b'3', b'0', b'.',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
];

#[no_mangle]
pub extern "C" fn _start() -> ! {
    // Register with driver manager
    register_with_driver_manager();

    // Initialize keyboard
    init_keyboard();

    // Register IRQ handler
    unsafe {
        sys_irq_register(KEYBOARD_IRQ);
    }

    // Register IPC port
    unsafe {
        sys_ipc_register_port(KEYBOARD_DRIVER_PORT);
    }

    // Spawn IRQ handler thread
    // For now, we'll handle both IRQ and IPC in the same loop

    // Main service loop
    loop {
        // Wait for IRQ
        unsafe {
            let irq = sys_irq_wait();
            if irq == KEYBOARD_IRQ {
                handle_keyboard_interrupt();
            }
        }

        // Check for IPC messages (non-blocking would be better)
        let mut msg = IpcMessage {
            sender_tid: 0,
            msg_type: 0,
            data: [0; 256],
        };

        unsafe {
            if sys_ipc_receive(KEYBOARD_DRIVER_PORT, &mut msg) == 0 {
                let response = handle_message(&msg);
                let _ = sys_ipc_send(msg.sender_tid, &response);
            }
        }
    }
}

fn register_with_driver_manager() {
    const DRIVER_MANAGER_PORT: u32 = 100;
    const MSG_REGISTER_DRIVER: u32 = 1;
    const DRIVER_TYPE_INPUT: u32 = 4;
    const KEYBOARD_DRIVER_PORT: u32 = 201; // Well-known port for keyboard driver
    
    // Create registration message
    let mut msg = IpcMessage {
        sender_tid: 0,  // Will be filled by kernel
        msg_type: 1,    // IPC_MSG_REQUEST
        data: [0; 256],
    };
    
    // Pack driver type (Input = 4) and port into message data
    msg.data[0] = DRIVER_TYPE_INPUT as u8;
    msg.data[1..5].copy_from_slice(&KEYBOARD_DRIVER_PORT.to_le_bytes());
    
    // Send registration message to driver manager
    unsafe {
        let _ = sys_ipc_send(DRIVER_MANAGER_PORT, &msg);
    }
}

fn init_keyboard() {
    unsafe {
        // Flush output buffer
        while (sys_io_read(KEYBOARD_STATUS_PORT, 1) & 0x01) != 0 {
            sys_io_read(KEYBOARD_DATA_PORT, 1);
        }

        // Enable keyboard
        sys_io_write(KEYBOARD_COMMAND_PORT, 0xAE, 1);
    }
}

fn handle_keyboard_interrupt() {
    unsafe {
        // Read scancode
        let scancode = sys_io_read(KEYBOARD_DATA_PORT, 1) as u8;

        // Convert to ASCII (simple mapping, ignores shift/ctrl/alt)
        if (scancode & 0x80) == 0 {
            // Key press (not release)
            if (scancode as usize) < SCANCODE_TO_ASCII.len() {
                let ascii = SCANCODE_TO_ASCII[scancode as usize];
                if ascii != 0 {
                    // Add to buffer
                    let next_head = (KEY_BUFFER_HEAD + 1) % KEY_BUFFER_SIZE;
                    if next_head != KEY_BUFFER_TAIL {
                        KEY_BUFFER[KEY_BUFFER_HEAD] = ascii;
                        KEY_BUFFER_HEAD = next_head;
                    }
                }
            }
        }
    }
}

fn handle_message(msg: &IpcMessage) -> IpcMessage {
    match msg.msg_type {
        MSG_KEYBOARD_GET_KEY => handle_get_key(),
        MSG_KEYBOARD_SET_LEDS => handle_set_leds(msg),
        _ => create_error_response(1),
    }
}

fn handle_get_key() -> IpcMessage {
    unsafe {
        if KEY_BUFFER_HEAD != KEY_BUFFER_TAIL {
            // Key available
            let key = KEY_BUFFER[KEY_BUFFER_TAIL];
            KEY_BUFFER_TAIL = (KEY_BUFFER_TAIL + 1) % KEY_BUFFER_SIZE;

            let mut response = IpcMessage {
                sender_tid: 0,
                msg_type: 0,
                data: [0; 256],
            };
            response.data[0] = key;
            response
        } else {
            // No key available
            create_error_response(2)
        }
    }
}

fn handle_set_leds(msg: &IpcMessage) -> IpcMessage {
    let leds = msg.data[0];

    unsafe {
        // Send LED command
        sys_io_write(KEYBOARD_DATA_PORT, 0xED, 1);
        // Wait for ACK
        while sys_io_read(KEYBOARD_DATA_PORT, 1) != 0xFA {}
        // Send LED state
        sys_io_write(KEYBOARD_DATA_PORT, leds as u32, 1);
    }

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
