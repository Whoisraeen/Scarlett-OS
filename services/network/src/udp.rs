//! UDP protocol implementation

/// UDP header structure
#[repr(C, packed)]
pub struct UdpHeader {
    pub src_port: u16,
    pub dest_port: u16,
    pub length: u16,
    pub checksum: u16,
    pub data: [u8; 0],  // Variable length data
}

/// Send UDP packet
pub fn udp_send(dest_ip: u32, dest_port: u16, src_port: u16, data: &[u8]) -> Result<(), ()> {
    // Build UDP header
    let mut udp_header = UdpHeader {
        src_port,
        dest_port,
        length: (8 + data.len()) as u16,
        checksum: 0, // Checksum would be calculated here
        data: [],
    };
    
    // Build packet
    let mut packet = [0u8; 1500];
    unsafe {
        core::ptr::copy_nonoverlapping(&udp_header as *const _ as *const u8, packet.as_mut_ptr(), 8);
    }
    let data_len = data.len().min(1492);
    packet[8..8+data_len].copy_from_slice(&data[0..data_len]);
    
    // Calculate checksum (simplified - would include pseudo-header)
    // For now, skip checksum calculation
    
    // Send via IP layer
    use crate::ip::ip_send;
    ip_send(dest_ip, crate::ip::IP_PROTOCOL_UDP, &packet[0..8+data_len])
}

/// Receive UDP packet
pub fn udp_receive(buffer: &mut [u8]) -> Result<(usize, u32, u16, u16), ()> {
    // Receive from IP layer
    use crate::ip::ip_receive;
    let mut ip_buffer = [0u8; 1500];
    match ip_receive(&mut ip_buffer) {
        Ok((len, src_ip, protocol)) => {
            if protocol == crate::ip::IP_PROTOCOL_UDP && len >= 8 {
                // Parse UDP header
                let udp_header = unsafe {
                    &*(ip_buffer.as_ptr() as *const UdpHeader)
                };
                
                // Copy data to buffer
                let data_len = (len - 8).min(buffer.len());
                buffer[0..data_len].copy_from_slice(&ip_buffer[8..8+data_len]);
                
                // Return data length, source IP, source port, dest port
                Ok((data_len, src_ip, udp_header.src_port, udp_header.dest_port))
            } else {
                Err(())
            }
        }
        Err(_) => Err(())
    }
}

