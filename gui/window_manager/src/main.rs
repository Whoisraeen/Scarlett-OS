/**
 * @file window_manager_service.rs
 * @brief User-space window manager service
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
}

#[repr(C)]
struct IpcMessage {
    sender_tid: u32,
    msg_type: u32,
    data: [u8; 256],
}

// Window Manager IPC port
const WINDOW_MANAGER_PORT: u32 = 200;

// Message types
const MSG_CREATE_WINDOW: u32 = 1;
const MSG_DESTROY_WINDOW: u32 = 2;
const MSG_MOVE_WINDOW: u32 = 3;
const MSG_RESIZE_WINDOW: u32 = 4;
const MSG_FOCUS_WINDOW: u32 = 5;
const MSG_MINIMIZE_WINDOW: u32 = 6;
const MSG_MAXIMIZE_WINDOW: u32 = 7;
const MSG_GET_WINDOW_LIST: u32 = 8;

#[repr(C)]
struct Window {
    id: u32,
    x: i32,
    y: i32,
    width: u32,
    height: u32,
    title: [u8; 64],
    owner_tid: u32,
    flags: u32,
}

const MAX_WINDOWS: usize = 256;
static mut WINDOWS: [Option<Window>; MAX_WINDOWS] = [None; MAX_WINDOWS];
static mut NEXT_WINDOW_ID: u32 = 1;
static mut FOCUSED_WINDOW: u32 = 0;

#[no_mangle]
pub extern "C" fn _start() -> ! {
    // Register IPC port
    unsafe {
        sys_ipc_register_port(WINDOW_MANAGER_PORT);
    }

    // Main service loop
    loop {
        let mut msg = IpcMessage {
            sender_tid: 0,
            msg_type: 0,
            data: [0; 256],
        };

        // Wait for IPC message
        unsafe {
            if sys_ipc_receive(WINDOW_MANAGER_PORT, &mut msg) == 0 {
                let response = handle_message(&msg);
                let _ = sys_ipc_send(msg.sender_tid, &response);
            }
        }
    }
}

fn handle_message(msg: &IpcMessage) -> IpcMessage {
    match msg.msg_type {
        MSG_CREATE_WINDOW => handle_create_window(msg),
        MSG_DESTROY_WINDOW => handle_destroy_window(msg),
        MSG_MOVE_WINDOW => handle_move_window(msg),
        MSG_RESIZE_WINDOW => handle_resize_window(msg),
        MSG_FOCUS_WINDOW => handle_focus_window(msg),
        MSG_MINIMIZE_WINDOW => handle_minimize_window(msg),
        MSG_MAXIMIZE_WINDOW => handle_maximize_window(msg),
        MSG_GET_WINDOW_LIST => handle_get_window_list(msg),
        _ => create_error_response(1), // Unknown message type
    }
}

fn handle_create_window(msg: &IpcMessage) -> IpcMessage {
    unsafe {
        // Find free window slot
        for i in 0..MAX_WINDOWS {
            if WINDOWS[i].is_none() {
                let window_id = NEXT_WINDOW_ID;
                NEXT_WINDOW_ID += 1;

                // Parse window parameters from message data
                let x = i32::from_le_bytes([msg.data[0], msg.data[1], msg.data[2], msg.data[3]]);
                let y = i32::from_le_bytes([msg.data[4], msg.data[5], msg.data[6], msg.data[7]]);
                let width = u32::from_le_bytes([msg.data[8], msg.data[9], msg.data[10], msg.data[11]]);
                let height = u32::from_le_bytes([msg.data[12], msg.data[13], msg.data[14], msg.data[15]]);

                let mut title = [0u8; 64];
                title[..32].copy_from_slice(&msg.data[16..48]);

                WINDOWS[i] = Some(Window {
                    id: window_id,
                    x,
                    y,
                    width,
                    height,
                    title,
                    owner_tid: msg.sender_tid,
                    flags: 0,
                });

                // Return window ID
                let mut response = IpcMessage {
                    sender_tid: 0,
                    msg_type: 0,
                    data: [0; 256],
                };
                response.data[0..4].copy_from_slice(&window_id.to_le_bytes());
                return response;
            }
        }

        // No free slots
        create_error_response(2)
    }
}

fn handle_destroy_window(msg: &IpcMessage) -> IpcMessage {
    let window_id = u32::from_le_bytes([msg.data[0], msg.data[1], msg.data[2], msg.data[3]]);

    unsafe {
        for i in 0..MAX_WINDOWS {
            if let Some(window) = &WINDOWS[i] {
                if window.id == window_id && window.owner_tid == msg.sender_tid {
                    WINDOWS[i] = None;
                    return create_success_response();
                }
            }
        }
    }

    create_error_response(3) // Window not found
}

fn handle_move_window(_msg: &IpcMessage) -> IpcMessage {
    // TODO: Implement window move
    create_success_response()
}

fn handle_resize_window(_msg: &IpcMessage) -> IpcMessage {
    // TODO: Implement window resize
    create_success_response()
}

fn handle_focus_window(msg: &IpcMessage) -> IpcMessage {
    let window_id = u32::from_le_bytes([msg.data[0], msg.data[1], msg.data[2], msg.data[3]]);
    unsafe {
        FOCUSED_WINDOW = window_id;
    }
    create_success_response()
}

fn handle_minimize_window(_msg: &IpcMessage) -> IpcMessage {
    // TODO: Implement window minimize
    create_success_response()
}

fn handle_maximize_window(_msg: &IpcMessage) -> IpcMessage {
    // TODO: Implement window maximize
    create_success_response()
}

fn handle_get_window_list(_msg: &IpcMessage) -> IpcMessage {
    // TODO: Return list of windows
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
        msg_type: 1, // Error
        data: [0; 256],
    };
    response.data[0..4].copy_from_slice(&error_code.to_le_bytes());
    response
}
