#![no_std]
#![no_main]

extern crate alloc;

use core::panic::PanicInfo;

// Syscall numbers
const SYS_WRITE: u64 = 1;
const SYS_IPC_SEND: u64 = 20;
const SYS_IPC_RECEIVE: u64 = 21;

// PCI Driver Port
const PCI_DRIVER_PORT: u32 = 101;

// PCI Messages
const MSG_PCI_FIND_DEVICE: u32 = 13;

#[repr(C)]
struct IpcMessage {
    sender_tid: u64,
    msg_type: u32,
    msg_id: u32,
    inline_size: u32,
    inline_data: [u8; 64],
}

impl IpcMessage {
    fn new() -> Self {
        IpcMessage {
            sender_tid: 0,
            msg_type: 0,
            msg_id: 0,
            inline_size: 0,
            inline_data: [0; 64],
        }
    }
}

#[no_mangle]
pub extern "C" fn _start() -> ! {
    print("WiFi Driver Starting...\n");

    // Find Intel WiFi Controller (AX200)
    // Vendor: 0x8086 (Intel), Device: 0x2723 (AX200)
    
    let vendor_id: u16 = 0x8086;
    let device_id: u16 = 0x2723;
    
    print("Searching for Intel AX200 WiFi Controller...\n");
    
    let mut msg = IpcMessage::new();
    msg.msg_type = 1; // REQUEST
    msg.msg_id = MSG_PCI_FIND_DEVICE;
    msg.inline_data[0..2].copy_from_slice(&vendor_id.to_le_bytes());
    msg.inline_data[2..4].copy_from_slice(&device_id.to_le_bytes());
    msg.inline_size = 4;
    
    unsafe {
        sys_ipc_send(PCI_DRIVER_PORT, &msg);
        sys_ipc_receive(PCI_DRIVER_PORT, &mut msg);
    }
    
    if msg.inline_data[0] != 0xFF {
        let bus = msg.inline_data[0];
        let dev = msg.inline_data[1];
        let func = msg.inline_data[2];
        print("WiFi Controller found!\n");
        // Initialize controller (iwlwifi style)
        print("Simulating iwlwifi initialization...\n");
        print("- Resetting device...\n");
        print("- Loading firmware (placeholder)...\n");
        print("- Configuring device registers...\n");
        print("- Initializing queues (TX/RX)...\n");
        print("- Enabling interrupts...\n");
        print("- WiFi controller initialized.\n");
    } else {
        print("WiFi Controller not found.\n");
    }

    loop {}
}

unsafe fn sys_ipc_send(port: u32, msg: *const IpcMessage) -> u64 {
    let ret: u64;
    core::arch::asm!(
        "syscall",
        in("rdi") SYS_IPC_SEND,
        in("rsi") port,
        in("rdx") msg,
        out("rax") ret,
        lateout("rcx") _,
        lateout("r11") _,
    );
    ret
}

unsafe fn sys_ipc_receive(port: u32, msg: *mut IpcMessage) -> u64 {
    let ret: u64;
    core::arch::asm!(
        "syscall",
        in("rdi") SYS_IPC_RECEIVE,
        in("rsi") port,
        in("rdx") msg,
        out("rax") ret,
        lateout("rcx") _,
        lateout("r11") _,
    );
    ret
}

fn print(s: &str) {
    unsafe {
        core::arch::asm!(
            "syscall",
            in("rdi") SYS_WRITE,
            in("rsi") 1, // stdout
            in("rdx") s.as_ptr(),
            in("r10") s.len(),
            lateout("rax") _,
            lateout("rcx") _,
            lateout("r11") _,
        );
    }
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    print("WiFi Driver Panicked!\n");
    loop {}
}
