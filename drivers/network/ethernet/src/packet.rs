//! Ethernet packet handling

use driver_framework::{DriverError, dma::DmaBuffer};
use driver_framework::mmio::MmioRegion;

// Network device IPC operations
pub const NET_DEV_OP_SEND: u64 = 1;
pub const NET_DEV_OP_RECEIVE: u64 = 2;
pub const NET_DEV_OP_GET_MAC: u64 = 3;
pub const NET_DEV_OP_SET_IP: u64 = 4;

/// Ethernet frame header (14 bytes)
#[repr(C, packed)]
pub struct EthernetHeader {
    pub dest_mac: [u8; 6],
    pub src_mac: [u8; 6],
    pub ethertype: u16,
}

/// Send Ethernet packet
pub fn send_packet(
    _mmio: &MmioRegion,
    _buffer: &DmaBuffer,
) -> Result<(), DriverError> {
    // TODO: Implement packet transmission
    // - Add packet to transmit ring
    // - Notify hardware
    // - Wait for completion
    
    Ok(())
}

/// Receive Ethernet packet
pub fn receive_packet(
    _mmio: &MmioRegion,
    _buffer: &mut DmaBuffer,
) -> Result<usize, DriverError> {
    // TODO: Implement packet reception
    // - Check receive ring
    // - Copy packet to buffer
    // - Return packet length
    
    Err(DriverError::DeviceNotFound) // No packet available
}

