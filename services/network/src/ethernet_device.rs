//! Ethernet device communication for network service

use crate::ipc::{IpcMessage, ipc_send, ipc_receive, sys_ipc_send, sys_ipc_receive};

/// Ethernet device service port
static mut ETHERNET_DEV_PORT: u64 = 0;

/// Set Ethernet device port
pub fn set_ethernet_device_port(port: u64) {
    unsafe {
        ETHERNET_DEV_PORT = port;
    }
}

/// Send packet via Ethernet device
pub fn send_packet(data: &[u8]) -> Result<(), ()> {
    unsafe {
        if ETHERNET_DEV_PORT == 0 {
            return Err(()); // Driver not available
        }
        
        let mut request = IpcMessage::new();
        request.msg_id = 1; // NET_DEV_OP_SEND
        request.msg_type = crate::ipc::IPC_MSG_REQUEST;
        
        // Copy packet data to inline_data (limited to 64 bytes)
        let copy_len = data.len().min(64);
        request.inline_data[0..copy_len].copy_from_slice(&data[0..copy_len]);
        request.inline_size = copy_len as u32;
        
        // For larger packets, data would be in request.buffer
        
        // Send request with retry logic
        let mut retries = 3;
        loop {
            match ipc_send(ETHERNET_DEV_PORT, &request) {
                Ok(_) => break,
                Err(_) => {
                    retries -= 1;
                    if retries == 0 {
                        return Err(()); // Failed after retries
                    }
                    crate::syscalls::sys_yield();
                }
            }
        }
        
        // Receive response with retry logic
        let mut response = IpcMessage::new();
        retries = 3;
        loop {
            match ipc_receive(ETHERNET_DEV_PORT, &mut response) {
                Ok(_) => break,
                Err(_) => {
                    retries -= 1;
                    if retries == 0 {
                        return Err(()); // Failed after retries
                    }
                    crate::syscalls::sys_yield();
                }
            }
        }
        
        // Check success
        if response.inline_size > 0 && response.inline_data[0] == 0 {
            Ok(())
        } else {
            Err(())
        }
    }
}

/// Receive packet from Ethernet device
/// Returns packet data in provided buffer
pub fn receive_packet(buffer: &mut [u8]) -> Result<usize, ()> {
    unsafe {
        if ETHERNET_DEV_PORT == 0 {
            return Err(());
        }
        
        let mut request = IpcMessage::new();
        request.msg_id = 2; // NET_DEV_OP_RECEIVE
        request.msg_type = crate::ipc::IPC_MSG_REQUEST;
        
        // Send request with retry logic
        let mut retries = 3;
        loop {
            match ipc_send(ETHERNET_DEV_PORT, &request) {
                Ok(_) => break,
                Err(_) => {
                    retries -= 1;
                    if retries == 0 {
                        return Err(()); // Failed after retries
                    }
                    crate::syscalls::sys_yield();
                }
            }
        }
        
        // Receive response with retry logic
        let mut response = IpcMessage::new();
        retries = 3;
        loop {
            match ipc_receive(ETHERNET_DEV_PORT, &mut response) {
                Ok(_) => break,
                Err(_) => {
                    retries -= 1;
                    if retries == 0 {
                        return Err(()); // Failed after retries
                    }
                    crate::syscalls::sys_yield();
                }
            }
        }
        
        // Extract packet data
        if response.inline_size > 0 && response.inline_data[0] != 1 {
            let copy_len = buffer.len().min(response.inline_size as usize);
            buffer[0..copy_len].copy_from_slice(&response.inline_data[0..copy_len]);
            Ok(copy_len)
        } else {
            Err(()) // No packet available
        }
    }
}

/// Get MAC address from Ethernet device
pub fn get_mac_address() -> Result<[u8; 6], ()> {
    unsafe {
        if ETHERNET_DEV_PORT == 0 {
            return Err(());
        }
        
        let mut request = IpcMessage::new();
        request.msg_id = 3; // NET_DEV_OP_GET_MAC
        request.msg_type = crate::ipc::IPC_MSG_REQUEST;
        
        // Send request with retry logic
        let mut retries = 3;
        loop {
            match ipc_send(ETHERNET_DEV_PORT, &request) {
                Ok(_) => break,
                Err(_) => {
                    retries -= 1;
                    if retries == 0 {
                        return Err(()); // Failed after retries
                    }
                    crate::syscalls::sys_yield();
                }
            }
        }
        
        // Receive response with retry logic
        let mut response = IpcMessage::new();
        retries = 3;
        loop {
            match ipc_receive(ETHERNET_DEV_PORT, &mut response) {
                Ok(_) => break,
                Err(_) => {
                    retries -= 1;
                    if retries == 0 {
                        return Err(()); // Failed after retries
                    }
                    crate::syscalls::sys_yield();
                }
            }
        }
        
        // Extract MAC address
        if response.inline_size >= 6 {
            let mut mac = [0u8; 6];
            mac.copy_from_slice(&response.inline_data[0..6]);
            Ok(mac)
        } else {
            Err(())
        }
    }
}

/// Set IP configuration on Ethernet device
pub fn set_ip_config(ip: u32, netmask: u32, gateway: u32) -> Result<(), ()> {
    unsafe {
        if ETHERNET_DEV_PORT == 0 {
            return Err(());
        }
        
        let mut request = IpcMessage::new();
        request.msg_id = 4; // NET_DEV_OP_SET_IP
        request.msg_type = crate::ipc::IPC_MSG_REQUEST;
        
        // Pack IP configuration
        request.inline_data[0..4].copy_from_slice(&ip.to_le_bytes());
        request.inline_data[4..8].copy_from_slice(&netmask.to_le_bytes());
        request.inline_data[8..12].copy_from_slice(&gateway.to_le_bytes());
        request.inline_size = 12;
        
        // Send request with retry logic
        let mut retries = 3;
        loop {
            match ipc_send(ETHERNET_DEV_PORT, &request) {
                Ok(_) => break,
                Err(_) => {
                    retries -= 1;
                    if retries == 0 {
                        return Err(()); // Failed after retries
                    }
                    crate::syscalls::sys_yield();
                }
            }
        }
        
        // Receive response with retry logic
        let mut response = IpcMessage::new();
        retries = 3;
        loop {
            match ipc_receive(ETHERNET_DEV_PORT, &mut response) {
                Ok(_) => break,
                Err(_) => {
                    retries -= 1;
                    if retries == 0 {
                        return Err(()); // Failed after retries
                    }
                    crate::syscalls::sys_yield();
                }
            }
        }
        
        // Check success
        if response.inline_size > 0 && response.inline_data[0] == 0 {
            Ok(())
        } else {
            Err(())
        }
    }
}

