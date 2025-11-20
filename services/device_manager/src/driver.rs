//! Driver loading and management

use crate::device::{Device, DeviceState, set_device_driver, set_device_state};
use crate::pci::PciDevice;
use crate::process_spawn::spawn_driver_process;

/// Driver information
pub struct DriverInfo {
    pub name: &'static str,
    pub probe: fn(&PciDevice) -> bool,
    pub load: fn(&PciDevice) -> Result<(), ()>,
}

/// Known drivers
static DRIVERS: &[DriverInfo] = &[
    DriverInfo {
        name: "ahci",
        probe: ahci_probe,
        load: ahci_load,
    },
    DriverInfo {
        name: "ethernet",
        probe: ethernet_probe,
        load: ethernet_load,
    },
];

/// Probe for AHCI controller
fn ahci_probe(device: &PciDevice) -> bool {
    device.class_code == 0x01 &&  // Mass storage controller
    device.subclass == 0x06 &&    // SATA AHCI
    device.interface == 0x01        // AHCI 1.0
}

/// Load AHCI driver
fn ahci_load(device: &PciDevice) -> Result<(), ()> {
    // Spawn AHCI driver process
    match spawn_driver_process("ahci") {
        Ok(driver_port) => {
            // Notify block device service about new driver
            let _ = crate::service_registry::notify_service(
                crate::service_registry::ServiceType::BlockDevice,
                driver_port
            );
            Ok(())
        }
        Err(_) => {
            // Failed to spawn driver process
            Err(())
        }
    }
}

/// Probe for Ethernet controller
fn ethernet_probe(device: &PciDevice) -> bool {
    device.class_code == 0x02 &&  // Network controller
    device.subclass == 0x00       // Ethernet
}

/// Load Ethernet driver
fn ethernet_load(device: &PciDevice) -> Result<(), ()> {
    // Spawn Ethernet driver process
    match spawn_driver_process("ethernet") {
        Ok(driver_port) => {
            // Notify network service about new driver
            let _ = crate::service_registry::notify_service(
                crate::service_registry::ServiceType::NetworkDevice,
                driver_port
            );
            Ok(())
        }
        Err(_) => {
            // Failed to spawn driver process
            Err(())
        }
    }
}

/// Find driver for device
pub fn find_driver(device: &PciDevice) -> Option<&'static DriverInfo> {
    DRIVERS.iter().find(|driver| (driver.probe)(device))
}

/// Load driver for device
pub fn load_driver(device: &PciDevice) -> Result<(), ()> {
    if let Some(driver) = find_driver(device) {
        // Set device driver name
        if let Some(dev) = crate::device::find_device_by_pci_id(device.vendor_id, device.device_id) {
            let device_id = dev.id;
            set_device_driver(device_id, driver.name).map_err(|_| ())?;
            set_device_state(device_id, DeviceState::Initialized).map_err(|_| ())?;
        }
        
        // Load driver
        (driver.load)(device)
    } else {
        Err(())
    }
}

/// Auto-load drivers for all devices
pub fn auto_load_drivers() {
    let count = crate::device::get_device_count();
    for i in 0..count {
        if let Some(device) = crate::device::get_device(i) {
            if device.driver_id == 0 {
                // Find PCI device
                if let Some(pci_dev) = crate::pci::pci_get_device_by_id(device.vendor_id, device.device_id) {
                    // Try to load driver
                    let _ = load_driver(&pci_dev);
                }
            }
        }
    }
}

