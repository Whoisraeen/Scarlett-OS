//! Network Service
//! 
//! User-space network stack service that provides TCP/IP networking
//! via IPC to other processes.

#![no_std]
#![no_main]

mod network;
mod ipc;
mod ethernet_device;

use core::panic::PanicInfo;
use network::network_init;
use ipc::{IpcMessage, sys_ipc_receive};
use ethernet_device::{set_ethernet_device_port, send_packet, receive_packet, get_mac_address, set_ip_config};

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

#[no_mangle]
pub extern "C" fn _start() -> ! {
    // Initialize network stack
    let _ = network_init();
    
    // Main service loop
    network_loop();
}

fn network_loop() {
    let mut msg = IpcMessage::new();
    let mut ethernet_port: Option<u64> = None;
    
    loop {
        // Receive IPC messages for network operations
        if sys_ipc_receive(3, &mut msg) == 0 {
            // Check for driver notification (from device manager)
            if msg.msg_id == 100 { // SERVICE_NOTIFY_DRIVER_AVAILABLE
                if msg.inline_size >= 8 {
                    let port = u64::from_le_bytes([
                        msg.inline_data[0], msg.inline_data[1], msg.inline_data[2], msg.inline_data[3],
                        msg.inline_data[4], msg.inline_data[5], msg.inline_data[6], msg.inline_data[7],
                    ]);
                    ethernet_port = Some(port);
                    set_ethernet_device_port(port);
                    
                    // Get MAC address and register device
                    if let Ok(mac) = get_mac_address() {
                        let _ = network::register_device(b"eth0", &mac);
                    }
                }
                continue;
            }
            
            // TODO: Handle socket creation requests
            // TODO: Handle connect, bind, listen, accept requests
            // TODO: Handle send, receive requests
            // TODO: Process network packets from drivers
            
            // For now, process received packets from Ethernet driver
            if ethernet_port.is_some() {
                let mut packet_buffer = [0u8; 1518];
                if let Ok(len) = receive_packet(&mut packet_buffer) {
                    // TODO: Process Ethernet packet (parse headers, route to protocol handlers)
                }
            }
        }
    }
}

