//! AHCI device identification

use crate::commands::{AhciCommand, execute_command};
use driver_framework::{DriverError, dma::DmaBuffer};
use driver_framework::mmio::MmioRegion;

/// ATA IDENTIFY device data structure (512 bytes)
#[repr(C, packed)]
pub struct AtaIdentifyData {
    pub general_config: u16,
    pub num_cylinders: u16,
    pub specific_config: u16,
    pub num_heads: u16,
    pub obsolete1: [u16; 2],
    pub num_sectors_per_track: u16,
    pub vendor_specific: [u16; 3],
    pub serial_number: [u8; 20],
    pub obsolete2: [u16; 2],
    pub firmware_revision: [u8; 8],
    pub model_number: [u8; 40],
    pub max_sectors_per_rw: u16,
    pub trusted_computing: u16,
    pub capabilities: u16,
    pub obsolete3: [u16; 2],
    pub pio_mode: u16,
    pub dma_mode: u16,
    pub fields_valid: u16,
    pub current_num_cylinders: u16,
    pub current_num_heads: u16,
    pub current_sectors_per_track: u16,
    pub current_capacity_sectors: u32,
    pub obsolete4: u16,
    pub total_sectors_28: u32,
    pub obsolete5: [u16; 2],
    pub total_sectors_48: u64,
    pub logical_sector_size: u16,
    pub physical_sector_size: u16,
    pub reserved: [u16; 164],
}

/// Port information
pub struct PortInfo {
    pub present: bool,
    pub lba48: bool,
    pub sectors: u64,
    pub sector_size: u32,
    pub model: [u8; 41],
    pub serial: [u8; 21],
}

/// Identify AHCI port device
pub fn identify_port(
    port_mmio: &MmioRegion,
    port_num: u8,
) -> Result<PortInfo, DriverError> {
    // Allocate DMA buffer for IDENTIFY data (512 bytes)
    let mut identify_buffer = DmaBuffer::alloc(512, 0)
        .map_err(|_| DriverError::OutOfMemory)?;
    
    // Get physical address
    let buffer_phys = identify_buffer.get_physical()
        .map_err(|_| DriverError::IoError)?;
    
    // Create and setup IDENTIFY command
    let mut cmd = AhciCommand::new()?;
    
    // Setup IDENTIFY command (ATA command 0xEC)
    unsafe {
        let cmd_list_ptr = cmd.cmd_list.as_mut_slice().as_mut_ptr() as *mut u8;
        let cmd_header = &mut *(cmd_list_ptr as *mut crate::ahci_structures::AhciCmdHeader);
        cmd_header.flags = (5 << 0) | (0 << 6); // CFL=5, Write=0 (read)
        cmd_header.prdtl = 1;
        cmd_header.ctba = (cmd.cmd_table_phys & 0xFFFFFFFF) as u32;
        cmd_header.ctbau = ((cmd.cmd_table_phys >> 32) & 0xFFFFFFFF) as u32;
        
        // Set up Command FIS
        let cmd_table_ptr = cmd.cmd_table.as_mut_slice().as_mut_ptr() as *mut u8;
        let cmd_table = &mut *(cmd_table_ptr as *mut crate::ahci_structures::AhciCmdTable);
        let fis = &mut *(cmd_table.cfis.as_mut_ptr() as *mut crate::ahci_structures::AhciFisH2D);
        
        fis.fis_type = crate::ahci_structures::FIS_TYPE_REG_H2D;
        fis.pmport_c = 0x80; // Command bit
        fis.command = 0xEC; // IDENTIFY DEVICE
        fis.device = 0x00; // Device/head register
        fis.count_low = 0;
        fis.count_high = 0;
        
        // Set up PRDT entry
        let prdt_ptr = cmd_table_ptr.add(128);
        let prdt = &mut *(prdt_ptr as *mut crate::ahci_structures::AhciPrdtEntry);
        prdt.dba = buffer_phys;
        prdt.dbc = 511; // 512 bytes minus 1
    }
    
    // Execute IDENTIFY command
    execute_command(port_mmio, port_num, &cmd)?;
    
    // Parse IDENTIFY data
    unsafe {
        let identify_data = &*(identify_buffer.as_mut_slice().as_ptr() as *const AtaIdentifyData);
        
        // Check if device is present (signature check would be done earlier)
        let mut info = PortInfo {
            present: true,
            lba48: false,
            sectors: 0,
            sector_size: 512,
            model: [0; 41],
            serial: [0; 21],
        };
        
        // Check for LBA48 support
        if (identify_data.capabilities & (1 << 9)) != 0 {
            // LBA supported
            if (identify_data.fields_valid & (1 << 10)) != 0 {
                // LBA48 supported
                info.lba48 = true;
                info.sectors = identify_data.total_sectors_48;
            } else {
                // LBA28 only
                info.sectors = identify_data.total_sectors_28 as u64;
            }
        } else {
            // CHS mode (legacy)
            info.sectors = (identify_data.num_cylinders as u64) *
                          (identify_data.num_heads as u64) *
                          (identify_data.num_sectors_per_track as u64);
        }
        
        // Get sector size
        if (identify_data.logical_sector_size & (1 << 14)) != 0 {
            // Logical sector size is valid
            let logical_size = identify_data.logical_sector_size & 0x3FFF;
            if logical_size > 0 {
                info.sector_size = logical_size as u32;
            }
        }
        
        // Copy model number (swap bytes, remove spaces)
        let model_bytes = identify_data.model_number;
        for i in 0..20 {
            let word = u16::from_le_bytes([model_bytes[i*2], model_bytes[i*2+1]]);
            let bytes = word.to_be_bytes();
            if bytes[0] != 0x20 && bytes[0] != 0x00 {
                if i*2 < 40 {
                    info.model[i*2] = bytes[0];
                }
                if i*2+1 < 40 {
                    info.model[i*2+1] = bytes[1];
                }
            }
        }
        info.model[40] = 0; // Null terminator
        
        // Copy serial number (swap bytes)
        let serial_bytes = identify_data.serial_number;
        for i in 0..10 {
            let word = u16::from_le_bytes([serial_bytes[i*2], serial_bytes[i*2+1]]);
            let bytes = word.to_be_bytes();
            if bytes[0] != 0x20 && bytes[0] != 0x00 {
                if i*2 < 20 {
                    info.serial[i*2] = bytes[0];
                }
                if i*2+1 < 20 {
                    info.serial[i*2+1] = bytes[1];
                }
            }
        }
        info.serial[20] = 0; // Null terminator
        
        Ok(info)
    }
}

