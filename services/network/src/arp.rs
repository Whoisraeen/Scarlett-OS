//! ARP (Address Resolution Protocol) Implementation

use crate::ethernet_device;
use core::mem;

/// ARP header structure
#[repr(C, packed)]
#[derive(Clone, Copy)]
pub struct ArpHeader {
    pub hardware_type: u16,     // Hardware type (Ethernet = 1)
    pub protocol_type: u16,     // Protocol type (IPv4 = 0x0800)
    pub hardware_addr_len: u8,  // Hardware address length (6 for MAC)
    pub protocol_addr_len: u8,  // Protocol address length (4 for IPv4)
    pub operation: u16,         // Operation (Request = 1, Reply = 2)
    pub sender_hw_addr: [u8; 6], // Sender MAC address
    pub sender_proto_addr: u32,  // Sender IP address
    pub target_hw_addr: [u8; 6], // Target MAC address
    pub target_proto_addr: u32,  // Target IP address
}

/// ARP operations
pub const ARP_OP_REQUEST: u16 = 1;
pub const ARP_OP_REPLY: u16 = 2;

/// ARP hardware types
pub const ARP_HW_ETHERNET: u16 = 1;

/// ARP protocol types
pub const ARP_PROTO_IPV4: u16 = 0x0800;

/// ARP cache entry
#[derive(Clone, Copy)]
pub struct ArpCacheEntry {
    pub ip: u32,
    pub mac: [u8; 6],
    pub timestamp: u64,
    pub valid: bool,
}

const ARP_CACHE_SIZE: usize = 256;
const ARP_CACHE_TIMEOUT: u64 = 300; // 5 minutes

static mut ARP_CACHE: [ArpCacheEntry; ARP_CACHE_SIZE] = [ArpCacheEntry {
    ip: 0,
    mac: [0; 6],
    timestamp: 0,
    valid: false,
}; ARP_CACHE_SIZE];

static mut ARP_CACHE_COUNT: usize = 0;
static mut LOCAL_IP: u32 = 0;
static mut LOCAL_MAC: [u8; 6] = [0; 6];

/// Initialize ARP
pub fn arp_init(local_ip: u32, local_mac: [u8; 6]) -> Result<(), ()> {
    unsafe {
        LOCAL_IP = local_ip;
        LOCAL_MAC = local_mac;
        ARP_CACHE_COUNT = 0;

        // Clear cache
        for i in 0..ARP_CACHE_SIZE {
            ARP_CACHE[i].valid = false;
        }
    }

    Ok(())
}

/// Send ARP request
pub fn arp_request(target_ip: u32) -> Result<(), ()> {
    unsafe {
        let mut arp = ArpHeader {
            hardware_type: ARP_HW_ETHERNET.to_be(),
            protocol_type: ARP_PROTO_IPV4.to_be(),
            hardware_addr_len: 6,
            protocol_addr_len: 4,
            operation: ARP_OP_REQUEST.to_be(),
            sender_hw_addr: LOCAL_MAC,
            sender_proto_addr: LOCAL_IP.to_be(),
            target_hw_addr: [0; 6],
            target_proto_addr: target_ip.to_be(),
        };

        // Build Ethernet frame
        let mut packet = [0u8; 64];

        // Ethernet header (14 bytes)
        // Destination MAC (broadcast)
        packet[0..6].copy_from_slice(&[0xFF; 6]);
        // Source MAC
        packet[6..12].copy_from_slice(&LOCAL_MAC);
        // EtherType (ARP = 0x0806)
        packet[12..14].copy_from_slice(&0x0806u16.to_be_bytes());

        // ARP payload
        let arp_bytes = core::slice::from_raw_parts(
            &arp as *const _ as *const u8,
            mem::size_of::<ArpHeader>()
        );
        packet[14..14 + mem::size_of::<ArpHeader>()].copy_from_slice(arp_bytes);

        // Send packet
        ethernet_device::send_packet(&packet[0..14 + mem::size_of::<ArpHeader>()])
    }
}

/// Send ARP reply
pub fn arp_reply(target_ip: u32, target_mac: [u8; 6]) -> Result<(), ()> {
    unsafe {
        let mut arp = ArpHeader {
            hardware_type: ARP_HW_ETHERNET.to_be(),
            protocol_type: ARP_PROTO_IPV4.to_be(),
            hardware_addr_len: 6,
            protocol_addr_len: 4,
            operation: ARP_OP_REPLY.to_be(),
            sender_hw_addr: LOCAL_MAC,
            sender_proto_addr: LOCAL_IP.to_be(),
            target_hw_addr: target_mac,
            target_proto_addr: target_ip.to_be(),
        };

        // Build Ethernet frame
        let mut packet = [0u8; 64];

        // Ethernet header
        packet[0..6].copy_from_slice(&target_mac);
        packet[6..12].copy_from_slice(&LOCAL_MAC);
        packet[12..14].copy_from_slice(&0x0806u16.to_be_bytes());

        // ARP payload
        let arp_bytes = core::slice::from_raw_parts(
            &arp as *const _ as *const u8,
            mem::size_of::<ArpHeader>()
        );
        packet[14..14 + mem::size_of::<ArpHeader>()].copy_from_slice(arp_bytes);

        ethernet_device::send_packet(&packet[0..14 + mem::size_of::<ArpHeader>()])
    }
}

/// Process received ARP packet
pub fn arp_process(packet: &[u8]) -> Result<(), ()> {
    if packet.len() < mem::size_of::<ArpHeader>() {
        return Err(());
    }

    unsafe {
        let arp = &*(packet.as_ptr() as *const ArpHeader);

        // Verify hardware and protocol types
        if u16::from_be(arp.hardware_type) != ARP_HW_ETHERNET ||
           u16::from_be(arp.protocol_type) != ARP_PROTO_IPV4 {
            return Err(());
        }

        let sender_ip = u32::from_be(arp.sender_proto_addr);
        let target_ip = u32::from_be(arp.target_proto_addr);
        let operation = u16::from_be(arp.operation);

        // Update ARP cache with sender info
        arp_cache_add(sender_ip, arp.sender_hw_addr);

        // Handle ARP request
        if operation == ARP_OP_REQUEST && target_ip == LOCAL_IP {
            // Send ARP reply
            arp_reply(sender_ip, arp.sender_hw_addr)?;
        }

        // Handle ARP reply (cache already updated above)
    }

    Ok(())
}

/// Add entry to ARP cache
fn arp_cache_add(ip: u32, mac: [u8; 6]) {
    unsafe {
        // Check if entry already exists
        for i in 0..ARP_CACHE_SIZE {
            if ARP_CACHE[i].valid && ARP_CACHE[i].ip == ip {
                ARP_CACHE[i].mac = mac;
                ARP_CACHE[i].timestamp = 0; // TODO: Get current time
                return;
            }
        }

        // Find free slot
        for i in 0..ARP_CACHE_SIZE {
            if !ARP_CACHE[i].valid {
                ARP_CACHE[i].ip = ip;
                ARP_CACHE[i].mac = mac;
                ARP_CACHE[i].timestamp = 0;
                ARP_CACHE[i].valid = true;
                ARP_CACHE_COUNT += 1;
                return;
            }
        }

        // Cache full, evict oldest entry
        let mut oldest_idx = 0;
        let mut oldest_time = ARP_CACHE[0].timestamp;
        for i in 1..ARP_CACHE_SIZE {
            if ARP_CACHE[i].timestamp < oldest_time {
                oldest_time = ARP_CACHE[i].timestamp;
                oldest_idx = i;
            }
        }

        ARP_CACHE[oldest_idx].ip = ip;
        ARP_CACHE[oldest_idx].mac = mac;
        ARP_CACHE[oldest_idx].timestamp = 0;
        ARP_CACHE[oldest_idx].valid = true;
    }
}

/// Lookup MAC address for IP
pub fn arp_lookup(ip: u32) -> Option<[u8; 6]> {
    unsafe {
        for i in 0..ARP_CACHE_SIZE {
            if ARP_CACHE[i].valid && ARP_CACHE[i].ip == ip {
                return Some(ARP_CACHE[i].mac);
            }
        }
    }

    None
}

/// Resolve IP to MAC (with ARP request if needed)
pub fn arp_resolve(ip: u32) -> Result<[u8; 6], ()> {
    // Check cache first
    if let Some(mac) = arp_lookup(ip) {
        return Ok(mac);
    }

    // Send ARP request
    arp_request(ip)?;

    // Wait for reply (with timeout)
    for _ in 0..100 {
        // Check cache again
        if let Some(mac) = arp_lookup(ip) {
            return Ok(mac);
        }

        // Yield CPU
        crate::syscalls::sys_yield();
    }

    Err(())
}
