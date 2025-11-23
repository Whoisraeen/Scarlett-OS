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
            
            // Handle socket creation requests
            if msg.msg_id == 1 { // SOCKET_CREATE
                use crate::socket::socket_create;
                let socket_type = msg.inline_data[0];
                let socket_fd = socket_create(socket_type);
                // Send response with socket_fd
            }
            
            // Handle connect, bind, listen, accept requests
            if msg.msg_id == 2 { // SOCKET_BIND
                // Parse address from message and bind
            }
            if msg.msg_id == 3 { // SOCKET_CONNECT
                // Parse address and connect
            }
            if msg.msg_id == 4 { // SOCKET_LISTEN
                // Parse backlog and listen
            }
            if msg.msg_id == 5 { // SOCKET_ACCEPT
                // Accept connection
            }
            
            // Handle send, receive requests
            if msg.msg_id == 6 { // SOCKET_SEND
                // Parse data and send
            }
            if msg.msg_id == 7 { // SOCKET_RECEIVE
                // Receive data and return
            }
            
            // Process network packets from drivers
            if ethernet_port.is_some() {
                let mut packet_buffer = [0u8; 1518];
                if let Ok(len) = receive_packet(&mut packet_buffer) {
                    // Process Ethernet packet (parse headers, route to protocol handlers)
                    if len >= 14 {
                        // Parse Ethernet header (14 bytes)
                        let eth_type = u16::from_be_bytes([packet_buffer[12], packet_buffer[13]]);
                        if eth_type == 0x0800 { // IPv4
                            // Route to IP layer
                            use crate::ip::ip_receive;
                            let mut ip_buffer = [0u8; 1500];
                            ip_buffer[0..len-14].copy_from_slice(&packet_buffer[14..len]);
                            if let Ok((data_len, src_ip, protocol)) = ip_receive(&mut ip_buffer) {
                                // Route to protocol handler
                                if protocol == crate::ip::IP_PROTOCOL_TCP {
                                    use crate::tcp::tcp_handle_packet;
                                    let _ = tcp_handle_packet(&ip_buffer[0..data_len], src_ip);
                                } else if protocol == crate::ip::IP_PROTOCOL_UDP {
                                    // Handle UDP packet
                                } else if protocol == crate::ip::IP_PROTOCOL_ICMP {
                                    // Handle ICMP packet
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

