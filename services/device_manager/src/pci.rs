//! PCI enumeration for device manager

use core::mem;

/// PCI device structure (must match kernel/drivers/pci/pci.h)
#[repr(C)]
pub struct PciDevice {
    pub bus: u8,
    pub device: u8,
    pub function: u8,
    pub vendor_id: u16,
    pub device_id: u16,
    pub class_code: u8,
    pub subclass: u8,
    pub prog_if: u8,
    pub header_type: u8,
    pub bars: [u64; 6],
}

/// PCI configuration space registers
pub const PCI_CONFIG_VENDOR_ID: u8 = 0x00;
pub const PCI_CONFIG_DEVICE_ID: u8 = 0x02;
pub const PCI_CONFIG_CLASS: u8 = 0x0B;
pub const PCI_CONFIG_SUBCLASS: u8 = 0x0A;
pub const PCI_CONFIG_PROG_IF: u8 = 0x09;
pub const PCI_CONFIG_HEADER_TYPE: u8 = 0x0E;
pub const PCI_CONFIG_BAR0: u8 = 0x10;

/// PCI class codes
pub const PCI_CLASS_MASS_STORAGE: u8 = 0x01;
pub const PCI_SUBCLASS_SATA: u8 = 0x06;
pub const PCI_PROG_IF_AHCI: u8 = 0x01;
pub const PCI_CLASS_NETWORK: u8 = 0x02;
pub const PCI_SUBCLASS_ETHERNET: u8 = 0x00;

const MAX_PCI_DEVICES: usize = 256;

/// PCI device list
static mut PCI_DEVICES: [PciDevice; MAX_PCI_DEVICES] = unsafe { mem::zeroed() };
static mut PCI_DEVICE_COUNT: usize = 0;

/// Read PCI configuration space (via syscall to kernel)
#[no_mangle]
pub extern "C" fn sys_pci_read_config(bus: u8, device: u8, function: u8, offset: u8) -> u32 {
    unsafe {
        syscall_raw(28, bus as u64, device as u64, function as u64, offset as u64, 0) as u32
    }
}

/// Enumerate PCI devices
pub fn pci_enumerate() -> Result<usize, ()> {
    unsafe {
        PCI_DEVICE_COUNT = 0;
        
        // Scan all buses, devices, and functions
        for bus in 0..256u16 {
            for device in 0..32u8 {
                let mut functions = 1u8;
                
                // Check if device exists
                let vendor_id = sys_pci_read_config(bus as u8, device, 0, PCI_CONFIG_VENDOR_ID);
                if (vendor_id & 0xFFFF) == 0xFFFF {
                    continue;  // Device doesn't exist
                }
                
                // Check if multi-function
                let header_type = sys_pci_read_config(bus as u8, device, 0, PCI_CONFIG_HEADER_TYPE) as u8;
                if (header_type & 0x80) != 0 {
                    functions = 8;
                }
                
                // Scan functions
                for function in 0..functions {
                    let vendor = sys_pci_read_config(bus as u8, device, function, PCI_CONFIG_VENDOR_ID);
                    if (vendor & 0xFFFF) == 0xFFFF {
                        continue;
                    }
                    
                    if PCI_DEVICE_COUNT >= MAX_PCI_DEVICES {
                        return Ok(PCI_DEVICE_COUNT);
                    }
                    
                    let dev = &mut PCI_DEVICES[PCI_DEVICE_COUNT];
                    dev.bus = bus as u8;
                    dev.device = device;
                    dev.function = function;
                    dev.vendor_id = (vendor & 0xFFFF) as u16;
                    dev.device_id = ((vendor >> 16) & 0xFFFF) as u16;
                    
                    let class_rev = sys_pci_read_config(bus as u8, device, function, 0x08);
                    dev.prog_if = ((class_rev >> 8) & 0xFF) as u8;
                    dev.subclass = ((class_rev >> 16) & 0xFF) as u8;
                    dev.class_code = ((class_rev >> 24) & 0xFF) as u8;
                    dev.header_type = sys_pci_read_config(bus as u8, device, function, PCI_CONFIG_HEADER_TYPE) as u8;
                    
                    // Read BARs
                    for i in 0..6 {
                        dev.bars[i] = sys_pci_read_config(bus as u8, device, function, PCI_CONFIG_BAR0 + (i * 4)) as u64;
                    }
                    
                    PCI_DEVICE_COUNT += 1;
                }
            }
        }
        
        Ok(PCI_DEVICE_COUNT)
    }
}

/// Get PCI device count
pub fn pci_get_device_count() -> usize {
    unsafe { PCI_DEVICE_COUNT }
}

/// Get PCI device by index
pub fn pci_get_device(index: usize) -> Option<&'static PciDevice> {
    unsafe {
        if index < PCI_DEVICE_COUNT {
            Some(&PCI_DEVICES[index])
        } else {
            None
        }
    }
}

/// Find PCI device by vendor/device ID
pub fn pci_find_device(vendor_id: u16, device_id: u16) -> Option<&'static PciDevice> {
    unsafe {
        for i in 0..PCI_DEVICE_COUNT {
            if PCI_DEVICES[i].vendor_id == vendor_id && PCI_DEVICES[i].device_id == device_id {
                return Some(&PCI_DEVICES[i]);
            }
        }
        None
    }
}

/// Find PCI device by class
pub fn pci_find_class(class_code: u8, subclass: u8, prog_if: u8) -> Option<&'static PciDevice> {
    unsafe {
        for i in 0..PCI_DEVICE_COUNT {
            if PCI_DEVICES[i].class_code == class_code &&
               PCI_DEVICES[i].subclass == subclass &&
               (prog_if == 0xFF || PCI_DEVICES[i].prog_if == prog_if) {
                return Some(&PCI_DEVICES[i]);
            }
        }
        None
    }
}

#[cfg(target_arch = "x86_64")]
unsafe fn syscall_raw(num: u64, arg1: u64, arg2: u64, arg3: u64, arg4: u64, arg5: u64) -> u64 {
    let ret: u64;
    core::arch::asm!(
        "syscall",
        in("rax") num,
        in("rdi") arg1,
        in("rsi") arg2,
        in("rdx") arg3,
        in("r10") arg4,
        in("r8") arg5,
        out("rax") ret,
        options(nostack, preserves_flags)
    );
    ret
}

#[cfg(not(target_arch = "x86_64"))]
unsafe fn syscall_raw(_num: u64, _arg1: u64, _arg2: u64, _arg3: u64, _arg4: u64, _arg5: u64) -> u64 {
    0
}

