#![no_std]
#![no_main]

//! PCI Bus Driver (User-Space)
//!
//! Enumerates PCI devices and provides access to PCI configuration space

extern crate alloc;

use alloc::vec::Vec;
use core::panic::PanicInfo;

mod ipc;
use ipc::{IpcMessage, sys_ipc_receive, sys_ipc_send, sys_ipc_register_port};

// PCI driver port
const PCI_DRIVER_PORT: u32 = 101;

// Driver Manager port
const DRIVER_MANAGER_PORT: u32 = 100;

// Message types
const MSG_PCI_READ_CONFIG: u32 = 10;
const MSG_PCI_WRITE_CONFIG: u32 = 11;
const MSG_PCI_ENUMERATE: u32 = 12;
const MSG_PCI_FIND_DEVICE: u32 = 13;

// PCI device information
#[repr(C)]
struct PciDevice {
    bus: u8,
    device: u8,
    function: u8,
    vendor_id: u16,
    device_id: u16,
    class_code: u8,
    subclass: u8,
    prog_if: u8,
    revision: u8,
    header_type: u8,
    bar: [u32; 6],
}

impl PciDevice {
    fn new() -> Self {
        PciDevice {
            bus: 0,
            device: 0,
            function: 0,
            vendor_id: 0,
            device_id: 0,
            class_code: 0,
            subclass: 0,
            prog_if: 0,
            revision: 0,
            header_type: 0,
            bar: [0; 6],
        }
    }
}

// PCI driver state
struct PciDriver {
    devices: Vec<PciDevice>,
}

impl PciDriver {
    fn new() -> Self {
        PciDriver {
            devices: Vec::new(),
        }
    }

    fn enumerate_devices(&mut self) {
        // Scan all PCI buses, devices, and functions
        for bus in 0..256 {
            for device in 0..32 {
                for function in 0..8 {
                    if let Some(pci_dev) = self.probe_device(bus as u8, device as u8, function as u8) {
                        self.devices.push(pci_dev);
                    }
                }
            }
        }
    }

    fn probe_device(&self, bus: u8, device: u8, function: u8) -> Option<PciDevice> {
        let vendor_id = self.read_config_word(bus, device, function, 0x00);
        
        // 0xFFFF means no device
        if vendor_id == 0xFFFF {
            return None;
        }

        let mut pci_dev = PciDevice::new();
        pci_dev.bus = bus;
        pci_dev.device = device;
        pci_dev.function = function;
        pci_dev.vendor_id = vendor_id;
        pci_dev.device_id = self.read_config_word(bus, device, function, 0x02);
        
        let class_rev = self.read_config_dword(bus, device, function, 0x08);
        pci_dev.revision = (class_rev & 0xFF) as u8;
        pci_dev.prog_if = ((class_rev >> 8) & 0xFF) as u8;
        pci_dev.subclass = ((class_rev >> 16) & 0xFF) as u8;
        pci_dev.class_code = ((class_rev >> 24) & 0xFF) as u8;
        
        let header = self.read_config_dword(bus, device, function, 0x0C);
        pci_dev.header_type = ((header >> 16) & 0xFF) as u8;

        // Read BARs
        for i in 0..6 {
            pci_dev.bar[i] = self.read_config_dword(bus, device, function, 0x10 + (i as u8 * 4));
        }

        Some(pci_dev)
    }

    fn read_config_word(&self, bus: u8, device: u8, function: u8, offset: u8) -> u16 {
        let dword = self.read_config_dword(bus, device, function, offset & 0xFC);
        ((dword >> ((offset & 2) * 8)) & 0xFFFF) as u16
    }

    fn read_config_dword(&self, bus: u8, device: u8, function: u8, offset: u8) -> u32 {
        let address = 0x80000000u32
            | ((bus as u32) << 16)
            | ((device as u32) << 11)
            | ((function as u32) << 8)
            | ((offset & 0xFC) as u32);

        unsafe {
            // Write address to CONFIG_ADDRESS (0xCF8)
            outl(0xCF8, address);
            // Read data from CONFIG_DATA (0xCFC)
            inl(0xCFC)
        }
    }

    fn write_config_dword(&self, bus: u8, device: u8, function: u8, offset: u8, value: u32) {
        let address = 0x80000000u32
            | ((bus as u32) << 16)
            | ((device as u32) << 11)
            | ((function as u32) << 8)
            | ((offset & 0xFC) as u32);

        unsafe {
            outl(0xCF8, address);
            outl(0xCFC, value);
        }
    }

    fn find_device(&self, vendor_id: u16, device_id: u16) -> Option<&PciDevice> {
        self.devices.iter()
            .find(|dev| dev.vendor_id == vendor_id && dev.device_id == device_id)
    }
}

// I/O port operations (need syscalls in real implementation)
unsafe fn outl(port: u16, value: u32) {
    // TODO: Use syscall to request I/O port access
    // For now, direct I/O (requires Ring 0 or IOPL=3)
    core::arch::asm!(
        "out dx, eax",
        in("dx") port,
        in("eax") value,
        options(nostack, preserves_flags)
    );
}

unsafe fn inl(port: u16) -> u32 {
    let value: u32;
    core::arch::asm!(
        "in eax, dx",
        in("dx") port,
        out("eax") value,
        options(nostack, preserves_flags)
    );
    value
}

static mut PCI_DRIVER: Option<PciDriver> = None;

#[no_mangle]
pub extern "C" fn _start() -> ! {
    pci_driver_init();
    pci_driver_loop();
}

fn pci_driver_init() {
    unsafe {
        PCI_DRIVER = Some(PciDriver::new());
    }

    // Register our IPC port
    if sys_ipc_register_port(PCI_DRIVER_PORT) != 0 {
        loop {}
    }

    // Register with driver manager
    let mut msg = IpcMessage::new();
    msg.msg_type = ipc::IPC_MSG_REQUEST;
    msg.msg_id = 1; // MSG_REGISTER_DRIVER
    msg.inline_data[0] = 1; // DriverType::PciBus
    msg.inline_data[1..5].copy_from_slice(&PCI_DRIVER_PORT.to_le_bytes());
    msg.inline_size = 5;

    if sys_ipc_send(DRIVER_MANAGER_PORT, &msg) != 0 {
        // Failed to register
        loop {}
    }

    // Enumerate PCI devices
    unsafe {
        if let Some(ref mut driver) = PCI_DRIVER {
            driver.enumerate_devices();
        }
    }
}

fn pci_driver_loop() -> ! {
    let mut msg = IpcMessage::new();

    loop {
        if sys_ipc_receive(PCI_DRIVER_PORT, &mut msg) == 0 {
            let response = handle_message(&msg);
            let _ = sys_ipc_send(msg.sender_tid, &response);
        }
    }
}

fn handle_message(msg: &IpcMessage) -> IpcMessage {
    let mut response = IpcMessage::new();
    response.msg_type = ipc::IPC_MSG_RESPONSE;
    response.msg_id = msg.msg_id;

    unsafe {
        if let Some(ref driver) = PCI_DRIVER {
            match msg.msg_id {
                MSG_PCI_READ_CONFIG => {
                    let bus = msg.inline_data[0];
                    let device = msg.inline_data[1];
                    let function = msg.inline_data[2];
                    let offset = msg.inline_data[3];

                    let value = driver.read_config_dword(bus, device, function, offset);
                    response.inline_data[0..4].copy_from_slice(&value.to_le_bytes());
                    response.inline_size = 4;
                }

                MSG_PCI_WRITE_CONFIG => {
                    let bus = msg.inline_data[0];
                    let device = msg.inline_data[1];
                    let function = msg.inline_data[2];
                    let offset = msg.inline_data[3];
                    let value = u32::from_le_bytes([
                        msg.inline_data[4],
                        msg.inline_data[5],
                        msg.inline_data[6],
                        msg.inline_data[7],
                    ]);

                    driver.write_config_dword(bus, device, function, offset, value);
                    response.inline_data[0] = 1; // Success
                    response.inline_size = 1;
                }

                MSG_PCI_ENUMERATE => {
                    let count = driver.devices.len().min(16);
                    response.inline_data[0] = count as u8;
                    response.inline_size = 1;
                }

                MSG_PCI_FIND_DEVICE => {
                    let vendor_id = u16::from_le_bytes([msg.inline_data[0], msg.inline_data[1]]);
                    let device_id = u16::from_le_bytes([msg.inline_data[2], msg.inline_data[3]]);

                    if let Some(dev) = driver.find_device(vendor_id, device_id) {
                        response.inline_data[0] = dev.bus;
                        response.inline_data[1] = dev.device;
                        response.inline_data[2] = dev.function;
                        response.inline_size = 3;
                    } else {
                        response.inline_data[0] = 0xFF; // Not found
                        response.inline_size = 1;
                    }
                }

                _ => {
                    response.inline_data[0] = 0xFF; // Unknown command
                    response.inline_size = 1;
                }
            }
        }
    }

    response
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}
