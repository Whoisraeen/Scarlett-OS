//! ICMP (Internet Control Message Protocol) Implementation

use crate::ip;
use core::mem;

/// ICMP header structure
#[repr(C, packed)]
#[derive(Clone, Copy)]
pub struct IcmpHeader {
    pub icmp_type: u8,
    pub code: u8,
    pub checksum: u16,
    pub rest_of_header: u32,  // Varies by type
}

/// ICMP types
pub const ICMP_TYPE_ECHO_REPLY: u8 = 0;
pub const ICMP_TYPE_DEST_UNREACHABLE: u8 = 3;
pub const ICMP_TYPE_SOURCE_QUENCH: u8 = 4;
pub const ICMP_TYPE_REDIRECT: u8 = 5;
pub const ICMP_TYPE_ECHO_REQUEST: u8 = 8;
pub const ICMP_TYPE_TIME_EXCEEDED: u8 = 11;
pub const ICMP_TYPE_PARAM_PROBLEM: u8 = 12;
pub const ICMP_TYPE_TIMESTAMP_REQUEST: u8 = 13;
pub const ICMP_TYPE_TIMESTAMP_REPLY: u8 = 14;

/// ICMP destination unreachable codes
pub const ICMP_CODE_NET_UNREACHABLE: u8 = 0;
pub const ICMP_CODE_HOST_UNREACHABLE: u8 = 1;
pub const ICMP_CODE_PROTOCOL_UNREACHABLE: u8 = 2;
pub const ICMP_CODE_PORT_UNREACHABLE: u8 = 3;
pub const ICMP_CODE_FRAGMENTATION_NEEDED: u8 = 4;

/// Calculate ICMP checksum
pub fn icmp_checksum(data: &[u8]) -> u16 {
    let mut sum: u32 = 0;
    let len = data.len();

    // Sum 16-bit words
    for i in (0..len).step_by(2) {
        if i + 1 < len {
            let word = u16::from_be_bytes([data[i], data[i + 1]]);
            sum += word as u32;
        } else {
            // Odd byte
            sum += (data[i] as u32) << 8;
        }
    }

    // Add carry
    while (sum >> 16) != 0 {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    !(sum as u16)
}

/// Send ICMP echo request (ping)
pub fn icmp_ping(dest_ip: u32, sequence: u16, data: &[u8]) -> Result<(), ()> {
    let mut packet = [0u8; 1500];

    // ICMP header
    let mut icmp = IcmpHeader {
        icmp_type: ICMP_TYPE_ECHO_REQUEST,
        code: 0,
        checksum: 0,
        rest_of_header: ((1u16.to_be() as u32) << 16) | (sequence.to_be() as u32), // ID=1, Sequence
    };

    // Copy header
    unsafe {
        let header_bytes = core::slice::from_raw_parts(
            &icmp as *const _ as *const u8,
            mem::size_of::<IcmpHeader>()
        );
        packet[0..mem::size_of::<IcmpHeader>()].copy_from_slice(header_bytes);
    }

    // Copy data
    let data_len = data.len().min(1500 - mem::size_of::<IcmpHeader>());
    packet[mem::size_of::<IcmpHeader>()..mem::size_of::<IcmpHeader>() + data_len]
        .copy_from_slice(&data[0..data_len]);

    let total_len = mem::size_of::<IcmpHeader>() + data_len;

    // Calculate checksum
    let checksum = icmp_checksum(&packet[0..total_len]);
    packet[2..4].copy_from_slice(&checksum.to_be_bytes());

    // Send via IP layer
    ip::ip_send(dest_ip, ip::IP_PROTOCOL_ICMP, &packet[0..total_len])
}

/// Send ICMP echo reply
pub fn icmp_pong(dest_ip: u32, id: u16, sequence: u16, data: &[u8]) -> Result<(), ()> {
    let mut packet = [0u8; 1500];

    // ICMP header
    let icmp = IcmpHeader {
        icmp_type: ICMP_TYPE_ECHO_REPLY,
        code: 0,
        checksum: 0,
        rest_of_header: ((id.to_be() as u32) << 16) | (sequence.to_be() as u32),
    };

    // Copy header
    unsafe {
        let header_bytes = core::slice::from_raw_parts(
            &icmp as *const _ as *const u8,
            mem::size_of::<IcmpHeader>()
        );
        packet[0..mem::size_of::<IcmpHeader>()].copy_from_slice(header_bytes);
    }

    // Copy data
    let data_len = data.len().min(1500 - mem::size_of::<IcmpHeader>());
    packet[mem::size_of::<IcmpHeader>()..mem::size_of::<IcmpHeader>() + data_len]
        .copy_from_slice(&data[0..data_len]);

    let total_len = mem::size_of::<IcmpHeader>() + data_len;

    // Calculate checksum
    let checksum = icmp_checksum(&packet[0..total_len]);
    packet[2..4].copy_from_slice(&checksum.to_be_bytes());

    // Send via IP layer
    ip::ip_send(dest_ip, ip::IP_PROTOCOL_ICMP, &packet[0..total_len])
}

/// Send ICMP destination unreachable
pub fn icmp_dest_unreachable(dest_ip: u32, code: u8, original_packet: &[u8]) -> Result<(), ()> {
    let mut packet = [0u8; 576]; // Minimum MTU

    // ICMP header
    let icmp = IcmpHeader {
        icmp_type: ICMP_TYPE_DEST_UNREACHABLE,
        code,
        checksum: 0,
        rest_of_header: 0, // Unused for dest unreachable
    };

    // Copy header
    unsafe {
        let header_bytes = core::slice::from_raw_parts(
            &icmp as *const _ as *const u8,
            mem::size_of::<IcmpHeader>()
        );
        packet[0..mem::size_of::<IcmpHeader>()].copy_from_slice(header_bytes);
    }

    // Copy original IP header + 8 bytes of data
    let copy_len = original_packet.len().min(576 - mem::size_of::<IcmpHeader>());
    packet[mem::size_of::<IcmpHeader>()..mem::size_of::<IcmpHeader>() + copy_len]
        .copy_from_slice(&original_packet[0..copy_len]);

    let total_len = mem::size_of::<IcmpHeader>() + copy_len;

    // Calculate checksum
    let checksum = icmp_checksum(&packet[0..total_len]);
    packet[2..4].copy_from_slice(&checksum.to_be_bytes());

    // Send via IP layer
    ip::ip_send(dest_ip, ip::IP_PROTOCOL_ICMP, &packet[0..total_len])
}

/// Process received ICMP packet
pub fn icmp_process(src_ip: u32, packet: &[u8]) -> Result<(), ()> {
    if packet.len() < mem::size_of::<IcmpHeader>() {
        return Err(());
    }

    unsafe {
        let icmp = &*(packet.as_ptr() as *const IcmpHeader);

        match icmp.icmp_type {
            ICMP_TYPE_ECHO_REQUEST => {
                // Extract ID and sequence
                let id = ((u32::from_be(icmp.rest_of_header) >> 16) & 0xFFFF) as u16;
                let sequence = (u32::from_be(icmp.rest_of_header) & 0xFFFF) as u16;

                // Get data
                let data = &packet[mem::size_of::<IcmpHeader>()..];

                // Send echo reply
                icmp_pong(src_ip, id, sequence, data)?;
            }
            ICMP_TYPE_ECHO_REPLY => {
                // Handle ping reply
                // Notify waiting application
                // In full implementation, would wake up waiting ping call
                // For now, just acknowledge receipt
            }
            ICMP_TYPE_DEST_UNREACHABLE => {
                // Handle destination unreachable
                // Notify appropriate connection
                // In full implementation, would notify TCP/UDP connections
                // For now, just log (would use logging service)
            }
            ICMP_TYPE_TIME_EXCEEDED => {
                // Handle time exceeded
                // Notify appropriate connection
                // In full implementation, would notify TCP/UDP connections
                // For now, just log (would use logging service)
            }
            _ => {
                // Unknown ICMP type
            }
        }
    }

    Ok(())
}
