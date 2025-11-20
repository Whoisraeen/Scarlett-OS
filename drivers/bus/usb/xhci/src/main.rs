#![no_std]
#![no_main]

//! XHCI (eXtensible Host Controller Interface) USB 3.0 Driver
//!
//! User-space driver for USB 3.0 host controllers.
//! Implements the XHCI specification for USB device communication.

extern crate driver_framework;

use driver_framework::{DriverResult, DriverError};
use driver_framework::mmio::MmioRegion;
use driver_framework::interrupts::IrqHandler;
use driver_framework::ipc::DriverIpc;

mod xhci_regs;
mod xhci_ring;
mod xhci_trb;
mod xhci_device;

use xhci_regs::*;
use xhci_ring::*;
use xhci_trb::*;

/// XHCI PCI Class codes
const PCI_CLASS_SERIAL: u8 = 0x0C;
const PCI_SUBCLASS_USB: u8 = 0x03;
const PCI_PROG_IF_XHCI: u8 = 0x30;

/// XHCI Vendor IDs
const PCI_VENDOR_INTEL: u16 = 0x8086;
const PCI_VENDOR_AMD: u16 = 0x1022;
const PCI_VENDOR_VIA: u16 = 0x1106;

/// Maximum number of device slots
const MAX_DEVICE_SLOTS: usize = 256;

/// Maximum number of interrupters
const MAX_INTERRUPTERS: usize = 8;

/// XHCI Driver State
pub struct XhciDriver {
    /// MMIO base address
    mmio_base: MmioRegion,

    /// Capability registers base
    cap_regs: *mut XhciCapabilityRegs,

    /// Operational registers base
    op_regs: *mut XhciOperationalRegs,

    /// Runtime registers base
    runtime_regs: *mut XhciRuntimeRegs,

    /// Doorbell registers base
    doorbell_regs: *mut u32,

    /// Device Context Base Address Array (DCBAA)
    dcbaa_phys: u64,
    dcbaa_virt: *mut u64,

    /// Command ring
    command_ring: CommandRing,

    /// Event ring
    event_ring: EventRing,

    /// Number of device slots
    max_slots: u8,

    /// Maximum ports
    max_ports: u8,

    /// IRQ handler
    irq_handler: Option<IrqHandler>,
}

impl XhciDriver {
    /// Create a new XHCI driver instance
    pub fn new() -> Self {
        Self {
            mmio_base: MmioRegion::new(0, 0),
            cap_regs: core::ptr::null_mut(),
            op_regs: core::ptr::null_mut(),
            runtime_regs: core::ptr::null_mut(),
            doorbell_regs: core::ptr::null_mut(),
            dcbaa_phys: 0,
            dcbaa_virt: core::ptr::null_mut(),
            command_ring: CommandRing::new(),
            event_ring: EventRing::new(),
            max_slots: 0,
            max_ports: 0,
            irq_handler: None,
        }
    }

    /// Initialize the XHCI controller
    pub fn init(&mut self, pci_bus: u8, pci_dev: u8, pci_func: u8) -> DriverResult<()> {
        // Read BAR0 from PCI configuration space
        let bar0 = self.read_pci_bar(pci_bus, pci_dev, pci_func, 0)?;

        // Map MMIO region
        self.mmio_base = MmioRegion::map(bar0, 0x10000)?;

        // Initialize register pointers
        self.init_registers()?;

        // Read capabilities
        self.read_capabilities()?;

        // Reset the controller
        self.reset()?;

        // Initialize device context base address array
        self.init_dcbaa()?;

        // Initialize command ring
        self.init_command_ring()?;

        // Initialize event ring
        self.init_event_ring()?;

        // Enable interrupts
        self.enable_interrupts()?;

        // Start the controller
        self.start()?;

        // Enumerate USB ports
        self.enumerate_ports()?;

        Ok(())
    }

    /// Initialize register pointers from MMIO base
    fn init_registers(&mut self) -> DriverResult<()> {
        let base = self.mmio_base.as_ptr() as usize;

        self.cap_regs = base as *mut XhciCapabilityRegs;

        // Read capability length to find operational registers
        let cap_length = unsafe { (*self.cap_regs).caplength };
        self.op_regs = (base + cap_length as usize) as *mut XhciOperationalRegs;

        // Read runtime register offset
        let rtsoff = unsafe { (*self.cap_regs).rtsoff };
        self.runtime_regs = (base + (rtsoff & !0x1F) as usize) as *mut XhciRuntimeRegs;

        // Read doorbell offset
        let dboff = unsafe { (*self.cap_regs).dboff };
        self.doorbell_regs = (base + (dboff & !0x3) as usize) as *mut u32;

        Ok(())
    }

    /// Read controller capabilities
    fn read_capabilities(&mut self) -> DriverResult<()> {
        unsafe {
            let hcsparams1 = (*self.cap_regs).hcsparams1;

            // Extract maximum device slots (bits 0-7)
            self.max_slots = (hcsparams1 & 0xFF) as u8;

            // Extract maximum ports (bits 24-31)
            self.max_ports = ((hcsparams1 >> 24) & 0xFF) as u8;
        }

        Ok(())
    }

    /// Reset the XHCI controller
    fn reset(&mut self) -> DriverResult<()> {
        unsafe {
            // Stop the controller if running
            (*self.op_regs).usbcmd &= !USBCMD_RUN_STOP;

            // Wait for controller to halt
            while ((*self.op_regs).usbsts & USBSTS_HCH) == 0 {
                // Spin wait
            }

            // Issue controller reset
            (*self.op_regs).usbcmd |= USBCMD_HCRST;

            // Wait for reset to complete
            while ((*self.op_regs).usbcmd & USBCMD_HCRST) != 0 {
                // Spin wait
            }

            // Wait for controller ready
            while ((*self.op_regs).usbsts & USBSTS_CNR) != 0 {
                // Spin wait
            }
        }

        Ok(())
    }

    /// Initialize Device Context Base Address Array
    fn init_dcbaa(&mut self) -> DriverResult<()> {
        // Allocate DCBAA (aligned to 64 bytes)
        let size = (self.max_slots as usize + 1) * 8;
        self.dcbaa_phys = self.alloc_dma(size, 64)?;
        self.dcbaa_virt = self.dcbaa_phys as *mut u64;

        // Zero initialize
        unsafe {
            core::ptr::write_bytes(self.dcbaa_virt, 0, self.max_slots as usize + 1);
        }

        // Program DCBAAP register
        unsafe {
            (*self.op_regs).dcbaap = self.dcbaa_phys;
        }

        Ok(())
    }

    /// Initialize command ring
    fn init_command_ring(&mut self) -> DriverResult<()> {
        self.command_ring.init()?;

        // Program CRCR register
        unsafe {
            (*self.op_regs).crcr = self.command_ring.get_phys_addr() | CRCR_RCS;
        }

        Ok(())
    }

    /// Initialize event ring
    fn init_event_ring(&mut self) -> DriverResult<()> {
        self.event_ring.init()?;

        // Program event ring registers in interrupter 0
        unsafe {
            let interrupter = &mut (*self.runtime_regs).interrupters[0];

            // Set event ring segment table size
            interrupter.erstsz = 1;

            // Set event ring segment table base address
            interrupter.erstba = self.event_ring.get_segment_table_addr();

            // Set event ring dequeue pointer
            interrupter.erdp = self.event_ring.get_dequeue_ptr();
        }

        Ok(())
    }

    /// Enable interrupts
    fn enable_interrupts(&mut self) -> DriverResult<()> {
        unsafe {
            // Enable interrupter 0
            let interrupter = &mut (*self.runtime_regs).interrupters[0];
            interrupter.iman |= IMAN_IE;

            // Enable USB interrupts
            (*self.op_regs).usbcmd |= USBCMD_INTE;
        }

        Ok(())
    }

    /// Start the XHCI controller
    fn start(&mut self) -> DriverResult<()> {
        unsafe {
            // Configure max device slots
            let config = (*self.op_regs).config;
            (*self.op_regs).config = (config & !0xFF) | (self.max_slots as u32);

            // Start the controller
            (*self.op_regs).usbcmd |= USBCMD_RUN_STOP;

            // Wait for controller to start
            while ((*self.op_regs).usbsts & USBSTS_HCH) != 0 {
                // Spin wait
            }
        }

        Ok(())
    }

    /// Enumerate USB ports
    fn enumerate_ports(&mut self) -> DriverResult<()> {
        for port in 0..self.max_ports {
            self.check_port(port)?;
        }

        Ok(())
    }

    /// Check a specific USB port for connected devices
    fn check_port(&mut self, port: u8) -> DriverResult<()> {
        unsafe {
            let portsc = self.read_port_register(port, 0);

            // Check if device is connected (CCS bit)
            if (portsc & PORTSC_CCS) != 0 {
                // Device connected - reset port
                self.reset_port(port)?;

                // Enable port
                self.enable_port(port)?;
            }
        }

        Ok(())
    }

    /// Reset a USB port
    fn reset_port(&mut self, port: u8) -> DriverResult<()> {
        unsafe {
            let mut portsc = self.read_port_register(port, 0);

            // Set port reset bit
            portsc |= PORTSC_PR;
            self.write_port_register(port, 0, portsc);

            // Wait for reset complete
            loop {
                portsc = self.read_port_register(port, 0);
                if (portsc & PORTSC_PR) == 0 {
                    break;
                }
            }
        }

        Ok(())
    }

    /// Enable a USB port
    fn enable_port(&mut self, port: u8) -> DriverResult<()> {
        unsafe {
            let mut portsc = self.read_port_register(port, 0);

            // Set port enable bit
            portsc |= PORTSC_PED;
            self.write_port_register(port, 0, portsc);
        }

        Ok(())
    }

    /// Read port register
    unsafe fn read_port_register(&self, port: u8, offset: usize) -> u32 {
        let port_base = (self.op_regs as usize) + 0x400 + (port as usize * 0x10);
        let reg_addr = (port_base + offset) as *const u32;
        core::ptr::read_volatile(reg_addr)
    }

    /// Write port register
    unsafe fn write_port_register(&mut self, port: u8, offset: usize, value: u32) {
        let port_base = (self.op_regs as usize) + 0x400 + (port as usize * 0x10);
        let reg_addr = (port_base + offset) as *mut u32;
        core::ptr::write_volatile(reg_addr, value);
    }

    /// Read PCI BAR
    fn read_pci_bar(&self, bus: u8, dev: u8, func: u8, bar: u8) -> DriverResult<u64> {
        // Use syscall to read PCI configuration space
        // This is a placeholder - actual implementation depends on syscall interface
        Ok(0xFEDC0000) // Example BAR address
    }

    /// Allocate DMA memory
    fn alloc_dma(&self, size: usize, align: usize) -> DriverResult<u64> {
        // Use syscall to allocate DMA memory
        // This is a placeholder - actual implementation depends on syscall interface
        Ok(0x1000000) // Example DMA address
    }
}

#[no_mangle]
pub extern "C" fn _start() -> ! {
    let mut driver = XhciDriver::new();

    // Initialize driver with PCI device information
    // These would normally come from device manager via IPC
    match driver.init(0, 0, 0) {
        Ok(_) => {
            // Driver initialized successfully
            // Enter main event loop
            loop {
                // Process events
            }
        }
        Err(_) => {
            // Driver initialization failed
            loop {}
        }
    }
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
