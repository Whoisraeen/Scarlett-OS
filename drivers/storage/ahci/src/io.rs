//! AHCI I/O operations

use crate::commands::{AhciCommand, execute_command};
use driver_framework::{DriverError, dma::DmaBuffer};
use driver_framework::mmio::MmioRegion;

/// Read sectors from AHCI port
pub fn read_sectors(
    port_mmio: &MmioRegion,
    port_num: u8,
    lba: u64,
    count: u32,
    buffer: &mut DmaBuffer,
    lba48: bool,
) -> Result<(), DriverError> {
    // Get physical address of buffer
    let buffer_phys = buffer.get_physical().map_err(|_| DriverError::IoError)?;
    
    // Create and setup command
    let mut cmd = AhciCommand::new()?;
    cmd.setup_read(lba, count, buffer_phys, lba48);
    
    // Execute command
    execute_command(port_mmio, port_num, &cmd)
}

/// Write sectors to AHCI port
pub fn write_sectors(
    port_mmio: &MmioRegion,
    port_num: u8,
    lba: u64,
    count: u32,
    buffer: &DmaBuffer,
    lba48: bool,
) -> Result<(), DriverError> {
    // Get physical address of buffer
    let buffer_phys = buffer.get_physical().map_err(|_| DriverError::IoError)?;
    
    // Create and setup command
    let mut cmd = AhciCommand::new()?;
    cmd.setup_write(lba, count, buffer_phys, lba48);
    
    // Execute command
    execute_command(port_mmio, port_num, &cmd)
}

