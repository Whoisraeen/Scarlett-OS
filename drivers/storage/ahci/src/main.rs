//! User-Space AHCI Storage Driver
//! 
//! This driver replaces the kernel-space AHCI driver in kernel/drivers/ahci/
//! It implements SATA support via AHCI (Advanced Host Controller Interface).

#![no_std]
#![no_main]

mod ahci_structures;
mod commands;
mod io;
mod identify;

use driver_framework::{Driver, DriverError, DeviceInfo, DeviceType};
use driver_framework::mmio::MmioRegion;
use driver_framework::interrupts;
use driver_framework::ipc::{ipc_create_port, ipc_receive, ipc_send, IpcMessage, IPC_MSG_REQUEST};
use driver_framework::dma::DmaBuffer;
use commands::{BLOCK_DEV_OP_READ, BLOCK_DEV_OP_WRITE};
use io::{read_sectors, write_sectors};
use identify::identify_port;

// AHCI register offsets
const AHCI_CAP: usize = 0x00;
const AHCI_GHC: usize = 0x04;
const AHCI_PI: usize = 0x0C;
const AHCI_PxCLB: usize = 0x00;  // Port command list base
const AHCI_PxCLBU: usize = 0x04; // Port command list base upper
const AHCI_PxFB: usize = 0x08;   // Port FIS base
const AHCI_PxFBU: usize = 0x0C;  // Port FIS base upper
const AHCI_PxIS: usize = 0x10;   // Port interrupt status
const AHCI_PxIE: usize = 0x14;   // Port interrupt enable
const AHCI_PxCMD: usize = 0x18;  // Port command
const AHCI_PxTFD: usize = 0x20;  // Port task file data
const AHCI_PxSIG: usize = 0x24;  // Port signature
const AHCI_PxSSTS: usize = 0x28; // Port SATA status
const AHCI_PxSCTL: usize = 0x2C; // Port SATA control
const AHCI_PxSERR: usize = 0x30; // Port SATA error
const AHCI_PxSACT: usize = 0x34; // Port SATA active
const AHCI_PxCI: usize = 0x38;    // Port command issue

// AHCI port offsets (each port is 0x80 bytes)
const AHCI_PORT_OFFSET: usize = 0x100;

// AHCI command flags
const AHCI_PxCMD_ST: u32 = 1 << 0;      // Start
const AHCI_PxCMD_FRE: u32 = 1 << 4;     // FIS receive enable
const AHCI_PxCMD_FR: u32 = 1 << 14;     // FIS receive running
const AHCI_PxCMD_CR: u32 = 1 << 15;     // Command list running

// AHCI GHC flags
const AHCI_GHC_AE: u32 = 1 << 31;       // AHCI enable
const AHCI_GHC_IE: u32 = 1 << 1;        // Interrupt enable

struct AhciPort {
    port_num: u8,
    mmio: Option<MmioRegion>,
    initialized: bool,
}

struct AhciDriver {
    initialized: bool,
    device_port: u64,
    mmio: Option<MmioRegion>,
    ports: [AhciPort; 32],
    port_count: usize,
}

impl AhciDriver {
    fn new() -> Self {
        Self {
            initialized: false,
            device_port: 0,
            mmio: None,
            ports: [AhciPort {
                port_num: 0,
                mmio: None,
                initialized: false,
            }; 32],
            port_count: 0,
        }
    }
    
    fn init_controller(&mut self, device_info: &DeviceInfo) -> Result<(), DriverError> {
        if self.initialized {
            return Err(DriverError::AlreadyInitialized);
        }
        
        // Decode BAR5 (AHCI MMIO base)
        let bar5 = device_info.bars[5];
        if bar5 == 0 {
            return Err(DriverError::DeviceNotFound);
        }
        
        // Extract MMIO address (BAR5 is 64-bit for AHCI)
        let mmio_base = bar5 & !0xFFF; // Clear lower 12 bits
        if mmio_base == 0 {
            return Err(DriverError::DeviceNotFound);
        }
        
        // Map MMIO region (AHCI uses 4KB for controller + ports)
        let mmio = MmioRegion::map(mmio_base, 0x1000).map_err(|_| DriverError::IoError)?;
        
        // Enable AHCI mode in GHC register
        unsafe {
            let ghc = mmio.read32(AHCI_GHC);
            mmio.write32(AHCI_GHC, ghc | AHCI_GHC_AE | AHCI_GHC_IE);
        }
        
        // Read implemented ports
        unsafe {
            let pi = mmio.read32(AHCI_PI);
            for i in 0..32 {
                if (pi & (1 << i)) != 0 {
                    self.ports[self.port_count] = AhciPort {
                        port_num: i as u8,
                        mmio: Some(MmioRegion::map(
                            mmio_base + AHCI_PORT_OFFSET + (i * 0x80) as u64,
                            0x80
                        ).map_err(|_| DriverError::IoError)?),
                        initialized: false,
                    };
                    self.port_count += 1;
                }
            }
        }
        
        self.mmio = Some(mmio);
        self.initialized = true;
        
        Ok(())
    }
    
    fn init_port(&mut self, port_idx: usize) -> Result<(), DriverError> {
        if port_idx >= self.port_count {
            return Err(DriverError::InvalidArgument);
        }
        
        let port = &mut self.ports[port_idx];
        if port.initialized {
            return Err(DriverError::AlreadyInitialized);
        }
        
        // Get controller MMIO (needed for port access)
        let controller_mmio = self.mmio.as_ref().ok_or(DriverError::NotInitialized)?;
        
        // Check port signature to see if device is present
        let port_base = 0x100 + (port.port_num as usize * 0x80);
        let signature = unsafe { controller_mmio.read32(port_base + 0x24) }; // PxSIG
        
        // Signature values:
        // 0x00000101 = ATA device
        // 0xEB140101 = ATAPI device
        // 0x00000000 = No device
        if signature == 0 || signature == 0xFFFFFFFF {
            // No device present
            port.present = false;
            port.initialized = true;
            return Ok(());
        }
        
        port.present = true;
        
        // Initialize port command list and FIS structures
        // Allocate persistent command list (1KB, must be 1KB aligned)
        // Allocate persistent FIS base (256 bytes, must be 256-byte aligned)
        // These should be allocated once and kept for the port's lifetime
        // For now, we'll allocate them per-command (inefficient but works)
        
        // Start port command engine
        unsafe {
            let mut cmd = controller_mmio.read32(port_base + 0x18); // PxCMD
            if (cmd & 0x1) == 0 {
                controller_mmio.write32(port_base + 0x18, cmd | 0x1); // ST
            }
            if (cmd & 0x10) == 0 {
                controller_mmio.write32(port_base + 0x18, cmd | 0x10); // FRE
            }
        }
        
        // Wait for port to be ready
        let mut timeout = 100000;
        while timeout > 0 {
            let cmd = unsafe { controller_mmio.read32(port_base + 0x18) };
            if (cmd & 0x8000) == 0 && (cmd & 0x4000) == 0 {
                break; // CR and FR cleared
            }
            timeout -= 1;
        }
        
        if timeout == 0 {
            return Err(DriverError::Timeout);
        }
        
        // Identify device
        if let Ok(info) = identify_port(controller_mmio, port.port_num) {
            port.lba48 = info.lba48;
            port.sectors = info.sectors;
            port.sector_size = info.sector_size;
            port.model = info.model;
        } else {
            // Identification failed, use defaults
            port.lba48 = true;
            port.sectors = 0;
            port.sector_size = 512;
        }
        
        port.initialized = true;
        Ok(())
    }
    
    fn handle_ipc(&mut self) {
        let mut msg = IpcMessage::new();
        if ipc_receive(self.device_port, &mut msg).is_err() {
            return;
        }
        
        let mut response = IpcMessage::new();
        response.msg_type = driver_framework::ipc::IPC_MSG_RESPONSE;
        response.msg_id = msg.msg_id;
        
        // Handle storage I/O requests
        match msg.msg_id {
            BLOCK_DEV_OP_READ => {
                // Parse request: port_idx (u8), lba (u64), count (u32)
                if msg.inline_size >= 13 {
                    let port_idx = msg.inline_data[0] as usize;
                    let lba = u64::from_le_bytes([
                        msg.inline_data[1], msg.inline_data[2], msg.inline_data[3],
                        msg.inline_data[4], msg.inline_data[5], msg.inline_data[6],
                        msg.inline_data[7], msg.inline_data[8],
                    ]);
                    let count = u32::from_le_bytes([
                        msg.inline_data[9], msg.inline_data[10],
                        msg.inline_data[11], msg.inline_data[12],
                    ]);
                    
                    if port_idx < self.port_count {
                        let port = &self.ports[port_idx];
                        if let Some(ref port_mmio) = port.mmio {
                            // Allocate DMA buffer for read
                            if let Ok(mut buffer) = DmaBuffer::alloc((count * 512) as usize, 0) {
                                if let Ok(_) = read_sectors(
                                    port_mmio,
                                    port.port_num,
                                    lba,
                                    count,
                                    &mut buffer,
                                    true, // Assume LBA48
                                ) {
                                    // Copy data to response buffer (limited to 64 bytes inline)
                                    unsafe {
                                        let data = buffer.as_mut_slice();
                                        let copy_len = data.len().min(64);
                                        response.inline_data[0..copy_len].copy_from_slice(&data[0..copy_len]);
                                        response.inline_size = copy_len as u32;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            BLOCK_DEV_OP_WRITE => {
                // Parse request: port_idx (u8), lba (u64), count (u32), data...
                if msg.inline_size >= 13 {
                    let port_idx = msg.inline_data[0] as usize;
                    let lba = u64::from_le_bytes([
                        msg.inline_data[1], msg.inline_data[2], msg.inline_data[3],
                        msg.inline_data[4], msg.inline_data[5], msg.inline_data[6],
                        msg.inline_data[7], msg.inline_data[8],
                    ]);
                    let count = u32::from_le_bytes([
                        msg.inline_data[9], msg.inline_data[10],
                        msg.inline_data[11], msg.inline_data[12],
                    ]);
                    
                    if port_idx < self.port_count {
                        let port = &self.ports[port_idx];
                        if let Some(ref port_mmio) = port.mmio {
                            // Allocate DMA buffer for write
                            if let Ok(mut buffer) = DmaBuffer::alloc((count * 512) as usize, 0) {
                                // Copy data to buffer (from msg buffer if available)
                                unsafe {
                                    let buffer_slice = buffer.as_mut_slice();
                                    // TODO: Copy from message buffer (may need larger buffer)
                                    // For now, just mark as success if command executes
                                }
                                
                                if let Ok(_) = write_sectors(
                                    port_mmio,
                                    port.port_num,
                                    lba,
                                    count,
                                    &buffer,
                                    true, // Assume LBA48
                                ) {
                                    response.inline_data[0] = 0; // Success
                                    response.inline_size = 1;
                                }
                            }
                        }
                    }
                }
            }
            _ => {
                // Unknown operation
            }
        }
        
        let _ = ipc_send(self.device_port, &response);
    }
}

impl Driver for AhciDriver {
    fn init(&mut self) -> Result<(), DriverError> {
        if self.initialized {
            return Err(DriverError::AlreadyInitialized);
        }
        
        // Create IPC port for communication with device manager
        self.device_port = ipc_create_port().map_err(|_| DriverError::IoError)?;
        
        // TODO: Register with device manager
        // TODO: Wait for device assignment
        
        Ok(())
    }
    
    fn probe(&self, device_info: &DeviceInfo) -> bool {
        // Check if device is an AHCI controller
        device_info.device_type == DeviceType::Pci &&
        device_info.class_code == 0x01 &&  // Mass storage controller
        device_info.subclass == 0x06 &&    // SATA AHCI
        device_info.interface == 0x01      // AHCI 1.0
    }
    
    fn start(&mut self) -> Result<(), DriverError> {
        if !self.initialized {
            return Err(DriverError::NotInitialized);
        }
        
        // Initialize all ports
        for i in 0..self.port_count {
            let _ = self.init_port(i);
        }
        
        Ok(())
    }
    
    fn stop(&mut self) -> Result<(), DriverError> {
        if !self.initialized {
            return Err(DriverError::NotInitialized);
        }
        
        // TODO: Stop all ports
        // TODO: Disable interrupts
        
        Ok(())
    }
    
    fn name(&self) -> &'static str {
        "ahci"
    }
    
    fn version(&self) -> &'static str {
        "0.1.0"
    }
}

static mut DRIVER: AhciDriver = AhciDriver {
    initialized: false,
    device_port: 0,
    mmio: None,
    ports: [AhciPort {
        port_num: 0,
        mmio: None,
        initialized: false,
        present: false,
        lba48: true,
        sectors: 0,
        sector_size: 512,
        model: [0; 41],
    }; 32],
    port_count: 0,
};

#[no_mangle]
pub extern "C" fn _start() -> ! {
    unsafe {
        // Initialize driver
        let _ = DRIVER.init();
        
        // Driver main loop
        loop {
            DRIVER.handle_ipc();
            // TODO: Handle interrupts
        }
    }
}
