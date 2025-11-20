//! Device registration and management

use core::mem;
use crate::pci::PciDevice;

/// Device types
#[repr(u8)]
pub enum DeviceType {
    Unknown = 0,
    Pci = 1,
    Usb = 2,
    I2c = 3,
    Spi = 4,
    Serial = 5,
}

/// Device state
#[repr(u8)]
pub enum DeviceState {
    Uninitialized = 0,
    Initialized = 1,
    Active = 2,
    Suspended = 3,
    Error = 4,
}

/// Device information structure
#[repr(C)]
pub struct Device {
    pub device_id: u32,
    pub device_type: u8,
    pub state: u8,
    pub driver_loaded: bool,
    pub driver_name: [u8; 32],
    pub pci_info: PciDevice,  // Store directly, use device_type to determine if valid
    // Add more device-specific info as needed
}

const MAX_DEVICES: usize = 256;

/// Device registry
static mut DEVICES: [Device; MAX_DEVICES] = unsafe { mem::zeroed() };
static mut DEVICE_COUNT: usize = 0;

/// Register a PCI device
pub fn register_pci_device(pci_dev: &PciDevice) -> Result<u32, ()> {
    unsafe {
        if DEVICE_COUNT >= MAX_DEVICES {
            return Err(());
        }
        
        let device = &mut DEVICES[DEVICE_COUNT];
        device.device_id = DEVICE_COUNT as u32;
        device.device_type = DeviceType::Pci as u8;
        device.state = DeviceState::Uninitialized as u8;
        device.driver_loaded = false;
        device.pci_info = *pci_dev;  // Copy PCI device info
        
        // Clear driver name
        for i in 0..32 {
            device.driver_name[i] = 0;
        }
        
        let id = DEVICE_COUNT as u32;
        DEVICE_COUNT += 1;
        Ok(id)
    }
}

/// Get device by ID
pub fn get_device(device_id: u32) -> Option<&'static Device> {
    unsafe {
        if (device_id as usize) < DEVICE_COUNT {
            Some(&DEVICES[device_id as usize])
        } else {
            None
        }
    }
}

/// Get device count
pub fn get_device_count() -> usize {
    unsafe { DEVICE_COUNT }
}

/// Find device by PCI vendor/device ID
pub fn find_device_by_pci_id(vendor_id: u16, device_id: u16) -> Option<&'static Device> {
    unsafe {
        for i in 0..DEVICE_COUNT {
            if DEVICES[i].device_type == DeviceType::Pci as u8 {
                let pci = &DEVICES[i].pci_info;
                if pci.vendor_id == vendor_id && pci.device_id == device_id {
                    return Some(&DEVICES[i]);
                }
            }
        }
        None
    }
}

/// Set device driver
pub fn set_device_driver(device_id: u32, driver_name: &str) -> Result<(), ()> {
    unsafe {
        if (device_id as usize) >= DEVICE_COUNT {
            return Err(());
        }
        
        let device = &mut DEVICES[device_id as usize];
        device.driver_loaded = true;
        
        // Copy driver name
        let name_bytes = driver_name.as_bytes();
        let copy_len = name_bytes.len().min(31);
        for i in 0..copy_len {
            device.driver_name[i] = name_bytes[i];
        }
        device.driver_name[copy_len] = 0;
        
        Ok(())
    }
}

/// Set device state
pub fn set_device_state(device_id: u32, state: DeviceState) -> Result<(), ()> {
    unsafe {
        if (device_id as usize) >= DEVICE_COUNT {
            return Err(());
        }
        
        DEVICES[device_id as usize].state = state as u8;
        Ok(())
    }
}

