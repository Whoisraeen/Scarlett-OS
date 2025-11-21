#![no_std]
#![no_main]

//! Security Service
//!
//! Manages capabilities, ACLs, and application sandboxing

mod capability;
mod acl;
mod sandbox;
mod ipc;

use capability::CapabilityManager;
use sandbox::SandboxManager;
use capability::{Capability, CapabilityType};
use ipc::{IpcMessage, IPC_MSG_RESPONSE, ipc_receive, ipc_send};

static mut CAP_MANAGER: Option<CapabilityManager> = None;
static mut SANDBOX_MANAGER: Option<SandboxManager> = None;

// Security IPC operation IDs
const SEC_OP_GRANT_CAP: u64 = 1;
const SEC_OP_REVOKE_CAP: u64 = 2;
const SEC_OP_CHECK_CAP: u64 = 3;
const SEC_OP_CREATE_SANDBOX: u64 = 10;
const SEC_OP_CHECK_ACCESS: u64 = 11;

#[no_mangle]
pub extern "C" fn _start() -> ! {
    unsafe {
        // Initialize managers
        CAP_MANAGER = Some(CapabilityManager::new());
        SANDBOX_MANAGER = Some(SandboxManager::new());

        // Main service loop
        main_loop();
    }
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

fn main_loop() -> ! {
    let mut msg = IpcMessage::new();

    loop {
        // Receive on well-known security port (3). In a fuller implementation,
        // this would be dynamically registered with the device manager.
        if ipc_receive(3, &mut msg).is_err() {
            continue;
        }

        let mut resp = IpcMessage::new();
        resp.msg_type = IPC_MSG_RESPONSE;
        resp.msg_id = msg.msg_id;

        match msg.msg_id {
            SEC_OP_GRANT_CAP => handle_grant(&msg, &mut resp),
            SEC_OP_REVOKE_CAP => handle_revoke(&msg, &mut resp),
            SEC_OP_CHECK_CAP => handle_check(&msg, &mut resp),
            SEC_OP_CREATE_SANDBOX => handle_create_sandbox(&msg, &mut resp),
            SEC_OP_CHECK_ACCESS => handle_check_access(&msg, &mut resp),
            _ => {
                resp.inline_data[0] = 0xFF; // Unknown op
                resp.inline_size = 1;
            }
        }

        // Use sender_tid as reply target; real implementation would map to a reply port.
        let _ = ipc_send(msg.sender_tid, &resp);
    }
}

fn parse_u32_le(bytes: &[u8]) -> u32 {
    let mut buf = [0u8; 4];
    let len = bytes.len().min(4);
    buf[..len].copy_from_slice(&bytes[..len]);
    u32::from_le_bytes(buf)
}

fn parse_u64_le(bytes: &[u8]) -> u64 {
    let mut buf = [0u8; 8];
    let len = bytes.len().min(8);
    buf[..len].copy_from_slice(&bytes[..len]);
    u64::from_le_bytes(buf)
}

fn handle_grant(msg: &IpcMessage, resp: &mut IpcMessage) {
    // inline_data layout: [pid:4][cap_type:1][resource:8]
    if msg.inline_size < 13 {
        resp.inline_data[0] = 0xFE;
        resp.inline_size = 1;
        return;
    }

    let pid = parse_u32_le(&msg.inline_data[0..4]);
    let cap_type = msg.inline_data[4];
    let resource = parse_u64_le(&msg.inline_data[5..13]);

    unsafe {
        if let Some(ref mut mgr) = CAP_MANAGER {
            let cap = Capability::new(cap_from_u8(cap_type), resource, pid);
            match mgr.grant(pid, cap) {
                Ok(idx) => {
                    resp.inline_data[0..4].copy_from_slice(&(idx as u32).to_le_bytes());
                    resp.inline_size = 4;
                }
                Err(_) => {
                    resp.inline_data[0] = 0x01;
                    resp.inline_size = 1;
                }
            }
        } else {
            resp.inline_data[0] = 0x01;
            resp.inline_size = 1;
        }
    }
}

fn handle_revoke(msg: &IpcMessage, resp: &mut IpcMessage) {
    // inline_data layout: [pid:4][cap_idx:4]
    if msg.inline_size < 8 {
        resp.inline_data[0] = 0xFE;
        resp.inline_size = 1;
        return;
    }

    let pid = parse_u32_le(&msg.inline_data[0..4]);
    let cap_idx = parse_u32_le(&msg.inline_data[4..8]) as usize;

    unsafe {
        if let Some(ref mut mgr) = CAP_MANAGER {
            if mgr.revoke(pid, cap_idx).is_ok() {
                resp.inline_data[0] = 0;
                resp.inline_size = 1;
            } else {
                resp.inline_data[0] = 0x01;
                resp.inline_size = 1;
            }
        } else {
            resp.inline_data[0] = 0x01;
            resp.inline_size = 1;
        }
    }
}

fn handle_check(msg: &IpcMessage, resp: &mut IpcMessage) {
    // inline_data layout: [pid:4][cap_type:1][resource:8]
    if msg.inline_size < 13 {
        resp.inline_data[0] = 0xFE;
        resp.inline_size = 1;
        return;
    }

    let pid = parse_u32_le(&msg.inline_data[0..4]);
    let cap_type = msg.inline_data[4];
    let resource = parse_u64_le(&msg.inline_data[5..13]);

    unsafe {
        if let Some(ref mgr) = CAP_MANAGER {
            let allowed = mgr.check(pid, cap_from_u8(cap_type), resource);
            resp.inline_data[0] = if allowed { 1 } else { 0 };
            resp.inline_size = 1;
        } else {
            resp.inline_data[0] = 0;
            resp.inline_size = 1;
        }
    }
}

fn handle_create_sandbox(msg: &IpcMessage, resp: &mut IpcMessage) {
    // inline_data layout: [pid:4][mode:1] (mode 0=default restricted, 1=permissive)
    if msg.inline_size < 5 {
        resp.inline_data[0] = 0xFE;
        resp.inline_size = 1;
        return;
    }

    let pid = parse_u32_le(&msg.inline_data[0..4]);
    let mode = msg.inline_data[4];

    unsafe {
        if let Some(ref mut mgr) = SANDBOX_MANAGER {
            let cfg = if mode == 1 {
                sandbox::SandboxConfig::new_permissive()
            } else {
                sandbox::SandboxConfig::new_default()
            };

            if mgr.create_sandbox(pid, cfg).is_ok() {
                resp.inline_data[0] = 0;
                resp.inline_size = 1;
            } else {
                resp.inline_data[0] = 0x01;
                resp.inline_size = 1;
            }
        } else {
            resp.inline_data[0] = 0x01;
            resp.inline_size = 1;
        }
    }
}

fn handle_check_access(msg: &IpcMessage, resp: &mut IpcMessage) {
    // inline_data layout: [pid:4][resource_type:1][resource_id (rest as string bytes)]
    if msg.inline_size < 6 {
        resp.inline_data[0] = 0xFE;
        resp.inline_size = 1;
        return;
    }

    let pid = parse_u32_le(&msg.inline_data[0..4]);
    let resource_type_byte = msg.inline_data[4];
    let resource_id_bytes = &msg.inline_data[5..msg.inline_size as usize];
    let resource_id_len = resource_id_bytes.iter().position(|&b| b == 0).unwrap_or(resource_id_bytes.len());
    let resource_id = core::str::from_utf8(&resource_id_bytes[..resource_id_len]).unwrap_or("");

    let resource_type = match resource_type_byte {
        0 => "file",
        1 => "network",
        2 => "device",
        3 => "fork",
        4 => "exec",
        _ => "",
    };

    unsafe {
        if let Some(ref mgr) = SANDBOX_MANAGER {
            let allowed = mgr.check_access(pid, resource_type, resource_id);
            resp.inline_data[0] = if allowed { 1 } else { 0 };
            resp.inline_size = 1;
        } else {
            resp.inline_data[0] = 0;
            resp.inline_size = 1;
        }
    }
}

fn cap_from_u8(val: u8) -> CapabilityType {
    match val {
        1 => CapabilityType::FileRead,
        2 => CapabilityType::FileWrite,
        3 => CapabilityType::FileExecute,
        4 => CapabilityType::FileDelete,
        5 => CapabilityType::DirectoryCreate,
        6 => CapabilityType::DirectoryList,
        10 => CapabilityType::NetworkSend,
        11 => CapabilityType::NetworkReceive,
        12 => CapabilityType::NetworkBind,
        13 => CapabilityType::NetworkListen,
        20 => CapabilityType::DeviceRead,
        21 => CapabilityType::DeviceWrite,
        22 => CapabilityType::DeviceControl,
        30 => CapabilityType::ProcessCreate,
        31 => CapabilityType::ProcessKill,
        32 => CapabilityType::ProcessDebug,
        40 => CapabilityType::MemoryAllocate,
        41 => CapabilityType::MemoryMap,
        42 => CapabilityType::MemoryDMA,
        50 => CapabilityType::IpcSend,
        51 => CapabilityType::IpcReceive,
        52 => CapabilityType::IpcCreatePort,
        60 => CapabilityType::SystemShutdown,
        61 => CapabilityType::SystemReboot,
        62 => CapabilityType::SystemTime,
        70 => CapabilityType::HardwareMMIO,
        71 => CapabilityType::HardwareIRQ,
        72 => CapabilityType::HardwareDMA,
        _ => CapabilityType::FileRead,
    }
}
