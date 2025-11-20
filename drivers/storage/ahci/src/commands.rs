//! AHCI command processing

use crate::ahci_structures::{AhciFisH2D, AhciCmdHeader, AhciCmdTable, AhciPrdtEntry, FIS_TYPE_REG_H2D};
use driver_framework::{DriverError, dma::DmaBuffer};
use driver_framework::mmio::MmioRegion;

// Block device IPC operations
pub const BLOCK_DEV_OP_READ: u64 = 1;
pub const BLOCK_DEV_OP_WRITE: u64 = 2;
pub const BLOCK_DEV_OP_GET_INFO: u64 = 3;

pub struct AhciCommand {
    pub cmd_list: DmaBuffer,
    pub fis_base: DmaBuffer,
    pub cmd_table: DmaBuffer,
    pub cmd_list_phys: u64,
    pub fis_base_phys: u64,
    pub cmd_table_phys: u64,
}

impl AhciCommand {
    pub fn new() -> Result<Self, DriverError> {
        // Allocate command list (1KB, must be 1KB aligned)
        let cmd_list = DmaBuffer::alloc(1024, 0).map_err(|_| DriverError::OutOfMemory)?;
        
        // Allocate FIS base (256 bytes, must be 256-byte aligned)
        let fis_base = DmaBuffer::alloc(256, 0).map_err(|_| DriverError::OutOfMemory)?;
        
        // Allocate command table (128 bytes + PRDT entry)
        let cmd_table_size = 128 + 16; // cmd_table + 1 PRDT entry
        let cmd_table = DmaBuffer::alloc(cmd_table_size, 0).map_err(|_| DriverError::OutOfMemory)?;
        
        // Get physical addresses
        let cmd_list_phys = cmd_list.get_physical()
            .map_err(|_| DriverError::IoError)?;
        let fis_base_phys = fis_base.get_physical()
            .map_err(|_| DriverError::IoError)?;
        let cmd_table_phys = cmd_table.get_physical()
            .map_err(|_| DriverError::IoError)?;
        
        Ok(Self {
            cmd_list,
            fis_base,
            cmd_table,
            cmd_list_phys,
            fis_base_phys,
            cmd_table_phys,
        })
    }
    
    pub fn setup_read(&mut self, lba: u64, count: u32, buffer_phys: u64, lba48: bool) {
        unsafe {
            // Clear command list
            let cmd_list_ptr = self.cmd_list.as_mut_slice().as_mut_ptr() as *mut u8;
            let cmd_header = unsafe { &mut *(cmd_list_ptr as *mut AhciCmdHeader) };
            cmd_header.flags = (5 << 0) | (0 << 6); // CFL=5, Write=0 (read)
            cmd_header.prdtl = 1;
            cmd_header.ctba = (self.cmd_table_phys & 0xFFFFFFFF) as u32;
            cmd_header.ctbau = ((self.cmd_table_phys >> 32) & 0xFFFFFFFF) as u32;
            
            // Set up Command FIS
            let cmd_table_ptr = self.cmd_table.as_mut_slice().as_mut_ptr() as *mut u8;
            let cmd_table = unsafe { &mut *(cmd_table_ptr as *mut AhciCmdTable) };
            let fis = unsafe { &mut *(cmd_table.cfis.as_mut_ptr() as *mut AhciFisH2D) };
            
            fis.fis_type = FIS_TYPE_REG_H2D;
            fis.pmport_c = 0x80; // Command bit
            fis.command = if lba48 { 0x25 } else { 0x20 }; // READ DMA EXT or READ DMA
            fis.device = 0x40; // LBA mode
            
            if lba48 {
                fis.lba_low = (lba & 0xFF) as u8;
                fis.lba_mid = ((lba >> 8) & 0xFF) as u8;
                fis.lba_high = ((lba >> 16) & 0xFF) as u8;
                fis.device = 0x40 | (((lba >> 24) & 0x0F) as u8);
                fis.lba_low_ext = ((lba >> 24) & 0xFF) as u8;
                fis.lba_mid_ext = ((lba >> 32) & 0xFF) as u8;
                fis.lba_high_ext = ((lba >> 40) & 0xFF) as u8;
                fis.count_low = (count & 0xFF) as u8;
                fis.count_high = ((count >> 8) & 0xFF) as u8;
            } else {
                fis.lba_low = (lba & 0xFF) as u8;
                fis.lba_mid = ((lba >> 8) & 0xFF) as u8;
                fis.lba_high = ((lba >> 16) & 0xFF) as u8;
                fis.device = 0x40 | (((lba >> 24) & 0x0F) as u8);
                fis.count_low = (count & 0xFF) as u8;
                fis.count_high = ((count >> 8) & 0xFF) as u8;
            }
            
            // Set up PRDT entry
            let prdt_ptr = cmd_table_ptr.add(128);
            let prdt = unsafe { &mut *(prdt_ptr as *mut AhciPrdtEntry) };
            prdt.dba = buffer_phys;
            prdt.dbc = (count * 512 - 1) as u32; // Byte count minus 1
        }
    }
    
    pub fn setup_write(&mut self, lba: u64, count: u32, buffer_phys: u64, lba48: bool) {
        unsafe {
            // Similar to read, but with write command
            let cmd_list_ptr = self.cmd_list.as_mut_slice().as_mut_ptr() as *mut u8;
            let cmd_header = unsafe { &mut *(cmd_list_ptr as *mut AhciCmdHeader) };
            cmd_header.flags = (5 << 0) | (1 << 6); // CFL=5, Write=1
            cmd_header.prdtl = 1;
            cmd_header.ctba = (self.cmd_table_phys & 0xFFFFFFFF) as u32;
            cmd_header.ctbau = ((self.cmd_table_phys >> 32) & 0xFFFFFFFF) as u32;
            
            // Set up Command FIS (same as read but with write command)
            let cmd_table_ptr = self.cmd_table.as_mut_slice().as_mut_ptr() as *mut u8;
            let cmd_table = unsafe { &mut *(cmd_table_ptr as *mut AhciCmdTable) };
            let fis = unsafe { &mut *(cmd_table.cfis.as_mut_ptr() as *mut AhciFisH2D) };
            
            fis.fis_type = FIS_TYPE_REG_H2D;
            fis.pmport_c = 0x80;
            fis.command = if lba48 { 0x35 } else { 0x30 }; // WRITE DMA EXT or WRITE DMA
            fis.device = 0x40;
            
            // Same LBA setup as read
            if lba48 {
                fis.lba_low = (lba & 0xFF) as u8;
                fis.lba_mid = ((lba >> 8) & 0xFF) as u8;
                fis.lba_high = ((lba >> 16) & 0xFF) as u8;
                fis.device = 0x40 | (((lba >> 24) & 0x0F) as u8);
                fis.lba_low_ext = ((lba >> 24) & 0xFF) as u8;
                fis.lba_mid_ext = ((lba >> 32) & 0xFF) as u8;
                fis.lba_high_ext = ((lba >> 40) & 0xFF) as u8;
                fis.count_low = (count & 0xFF) as u8;
                fis.count_high = ((count >> 8) & 0xFF) as u8;
            } else {
                fis.lba_low = (lba & 0xFF) as u8;
                fis.lba_mid = ((lba >> 8) & 0xFF) as u8;
                fis.lba_high = ((lba >> 16) & 0xFF) as u8;
                fis.device = 0x40 | (((lba >> 24) & 0x0F) as u8);
                fis.count_low = (count & 0xFF) as u8;
                fis.count_high = ((count >> 8) & 0xFF) as u8;
            }
            
            // Set up PRDT entry
            let prdt_ptr = cmd_table_ptr.add(128);
            let prdt = unsafe { &mut *(prdt_ptr as *mut AhciPrdtEntry) };
            prdt.dba = buffer_phys;
            prdt.dbc = (count * 512 - 1) as u32;
        }
    }
}

pub fn execute_command(
    port_mmio: &MmioRegion,
    port_num: u8,
    cmd: &AhciCommand,
) -> Result<(), DriverError> {
    let port_base = 0x100 + (port_num as usize * 0x80);
    
    // Program port registers
    unsafe {
        port_mmio.write32(port_base + 0x00, (cmd.cmd_list_phys & 0xFFFFFFFF) as u32);
        port_mmio.write32(port_base + 0x04, ((cmd.cmd_list_phys >> 32) & 0xFFFFFFFF) as u32);
        port_mmio.write32(port_base + 0x08, (cmd.fis_base_phys & 0xFFFFFFFF) as u32);
        port_mmio.write32(port_base + 0x0C, ((cmd.fis_base_phys >> 32) & 0xFFFFFFFF) as u32);
        
        // Start command engine
        let mut cmd_reg = port_mmio.read32(port_base + 0x18);
        if (cmd_reg & 0x1) == 0 {
            port_mmio.write32(port_base + 0x18, cmd_reg | 0x1); // ST
        }
        if (cmd_reg & 0x10) == 0 {
            port_mmio.write32(port_base + 0x18, cmd_reg | 0x10); // FRE
        }
        
        // Issue command
        port_mmio.write32(port_base + 0x38, 1); // PxCI
        
        // Wait for completion
        let mut timeout = 1000000;
        while timeout > 0 {
            let ci = port_mmio.read32(port_base + 0x38);
            if (ci & 1) == 0 {
                break; // Command completed
            }
            timeout -= 1;
        }
        
        if timeout == 0 {
            return Err(DriverError::Timeout);
        }
        
        // Check for errors
        let tfd = port_mmio.read32(port_base + 0x20);
        if (tfd & 0x01) != 0 {
            return Err(DriverError::IoError);
        }
    }
    
    Ok(())
}

