//! Network stack implementation for network service

use core::mem;

/// Network device structure
#[repr(C)]
pub struct NetDevice {
    pub name: [u8; 32],
    pub mac_address: [u8; 6],
    pub ip_address: u32,
    pub netmask: u32,
    pub gateway: u32,
    pub mtu: u16,
    pub next: u64,  // Pointer to next device
}

/// IP packet structure
#[repr(C, packed)]
pub struct IpPacket {
    pub version_ihl: u8,
    pub tos: u8,
    pub total_length: u16,
    pub identification: u16,
    pub flags_fragment: u16,
    pub ttl: u8,
    pub protocol: u8,
    pub checksum: u16,
    pub src_ip: u32,
    pub dst_ip: u32,
    pub data: [u8; 0],  // Variable length
}

const MAX_DEVICES: usize = 16;

static mut NET_DEVICES: [NetDevice; MAX_DEVICES] = unsafe { mem::zeroed() };
static mut DEVICE_COUNT: usize = 0;
static mut INITIALIZED: bool = false;

/// Initialize network stack
pub fn network_init() -> Result<(), ()> {
    unsafe {
        if INITIALIZED {
            return Ok(());
        }
        
        DEVICE_COUNT = 0;
        INITIALIZED = true;
        
        Ok(())
    }
}

/// Register network device
pub fn register_device(name: &[u8], mac: &[u8; 6]) -> Result<usize, ()> {
    unsafe {
        if DEVICE_COUNT >= MAX_DEVICES {
            return Err(());
        }
        
        let device = &mut NET_DEVICES[DEVICE_COUNT];
        
        // Copy name
        let name_len = name.len().min(31);
        device.name[0..name_len].copy_from_slice(&name[0..name_len]);
        device.name[name_len] = 0;
        
        // Copy MAC address
        device.mac_address.copy_from_slice(mac);
        
        device.ip_address = 0;
        device.netmask = 0;
        device.gateway = 0;
        device.mtu = 1500;
        device.next = 0;
        
        let idx = DEVICE_COUNT;
        DEVICE_COUNT += 1;
        
        Ok(idx)
    }
}

/// Set IP configuration
pub fn set_ip_config(device_idx: usize, ip: u32, netmask: u32, gateway: u32) -> Result<(), ()> {
    unsafe {
        if device_idx >= DEVICE_COUNT {
            return Err(());
        }
        
        let device = &mut NET_DEVICES[device_idx];
        device.ip_address = ip;
        device.netmask = netmask;
        device.gateway = gateway;
        
        Ok(())
    }
}

/// Get network device
pub fn get_device(idx: usize) -> Option<&'static NetDevice> {
    unsafe {
        if idx < DEVICE_COUNT {
            Some(&NET_DEVICES[idx])
        } else {
            None
        }
    }
}

/// Get device count
pub fn get_device_count() -> usize {
    unsafe { DEVICE_COUNT }
}

