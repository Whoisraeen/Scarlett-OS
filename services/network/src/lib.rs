//! Network Service Library

pub mod network;
pub mod ethernet_device;
pub mod syscalls;

pub use network::{network_init, register_device, set_ip_config, get_device, get_device_count};
pub use ethernet_device::{set_ethernet_device_port, send_packet, receive_packet, get_mac_address, set_ip_config as set_ethernet_ip};
