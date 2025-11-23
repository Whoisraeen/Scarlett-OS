//! IP protocol implementation

use core::mem;

/// IP header structure
#[repr(C, packed)]
pub struct IpHeader {
    pub version_ihl: u8,      // Version (4 bits) + IHL (4 bits)
    pub tos: u8,              // Type of Service
    pub total_length: u16,    // Total length
    pub identification: u16,  // Identification
    pub flags_fragment: u16,  // Flags (3 bits) + Fragment offset (13 bits)
    pub ttl: u8,              // Time to Live
    pub protocol: u8,         // Protocol
    pub checksum: u16,        // Header checksum
    pub src_ip: u32,          // Source IP address
    pub dst_ip: u32,          // Destination IP address
    pub data: [u8; 0],        // Variable length data
}

/// IP protocol numbers
pub const IP_PROTOCOL_ICMP: u8 = 1;
pub const IP_PROTOCOL_TCP: u8 = 6;
pub const IP_PROTOCOL_UDP: u8 = 17;

/// Calculate IP checksum
pub fn ip_checksum(header: &IpHeader) -> u16 {
    let mut sum: u32 = 0;
    let header_len = ((header.version_ihl & 0x0F) * 4) as usize;
    let words = unsafe {
        core::slice::from_raw_parts(header as *const _ as *const u16, header_len / 2)
    };
    
    for &word in words {
        sum += u16::from_be_bytes(word.to_le_bytes()) as u32;
    }
    
    while (sum >> 16) != 0 {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    !(sum as u16).to_be()
}

/// Send IP packet
pub fn ip_send(dest_ip: u32, protocol: u8, data: &[u8]) -> Result<(), ()> {
    // Get network device via IPC
    // For now, use first available device
    // Allocate packet buffer
    let mut packet = [0u8; 1500];
    
    // Build IP header
    let mut ip_header = IpHeader {
        version_ihl: 0x45, // IPv4, 5 * 4 = 20 bytes header
        tos: 0,
        total_length: (20 + data.len()) as u16,
        identification: 0,
        flags_fragment: 0,
        ttl: 64,
        protocol,
        checksum: 0,
        src_ip: 0, // Would get from network device
        dst_ip: dest_ip,
    };
    
    // Calculate checksum
    ip_header.checksum = ip_checksum(&ip_header);
    
    // Copy header and data to packet
    unsafe {
        core::ptr::copy_nonoverlapping(&ip_header as *const _ as *const u8, packet.as_mut_ptr(), 20);
    }
    let data_len = data.len().min(1480);
    packet[20..20+data_len].copy_from_slice(&data[0..data_len]);
    
    // Send via Ethernet
    use crate::ethernet_device::send_packet;
    let _ = send_packet(&packet[0..20+data_len]);
    
    Ok(())
}

/// Receive IP packet
pub fn ip_receive(buffer: &mut [u8]) -> Result<(usize, u32, u8), ()> {
    // Receive from Ethernet layer
    use crate::ethernet_device::receive_packet;
    let mut eth_buffer = [0u8; 1518];
    match receive_packet(&mut eth_buffer) {
        Ok(len) => {
            if len < 20 {
                return Err(());
            }
            // Parse IP header
            let ip_header = unsafe {
                &*(eth_buffer.as_ptr().add(14) as *const IpHeader) // Skip 14-byte Ethernet header
            };
            
            // Verify IP version
            if (ip_header.version_ihl >> 4) != 4 {
                return Err(());
            }
            
            // Extract data
            let header_len = ((ip_header.version_ihl & 0x0F) * 4) as usize;
            let data_len = (ip_header.total_length as usize).saturating_sub(header_len);
            let copy_len = data_len.min(buffer.len());
            buffer[0..copy_len].copy_from_slice(&eth_buffer[14+header_len..14+header_len+copy_len]);
            
            // Return data length, source IP, protocol
            Ok((copy_len, ip_header.src_ip, ip_header.protocol))
        }
        Err(_) => Err(())
    }
}

