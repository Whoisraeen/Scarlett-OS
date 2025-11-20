//! AHCI data structures

// FIS types
pub const FIS_TYPE_REG_H2D: u8 = 0x27;
pub const FIS_TYPE_REG_D2H: u8 = 0x34;
pub const FIS_TYPE_DMA_ACTIVATE: u8 = 0x39;
pub const FIS_TYPE_DMA_SETUP: u8 = 0x41;
pub const FIS_TYPE_DATA: u8 = 0x46;
pub const FIS_TYPE_BIST: u8 = 0x58;
pub const FIS_TYPE_PIO_SETUP: u8 = 0x5F;
pub const FIS_TYPE_DEV_BITS: u8 = 0xA1;

// Command FIS (Host to Device) - 20 bytes
#[repr(C, packed)]
pub struct AhciFisH2D {
    pub fis_type: u8,        // 0x27
    pub pmport_c: u8,        // Port multiplier + command bit
    pub command: u8,         // ATA command
    pub features: u8,        // Features
    pub lba_low: u8,         // LBA low
    pub lba_mid: u8,         // LBA mid
    pub lba_high: u8,        // LBA high
    pub device: u8,          // Device/head
    pub lba_low_ext: u8,     // LBA low extended
    pub lba_mid_ext: u8,     // LBA mid extended
    pub lba_high_ext: u8,    // LBA high extended
    pub features_ext: u8,    // Features extended
    pub count_low: u8,       // Count low
    pub count_high: u8,      // Count high
    pub icc: u8,             // Isochronous command completion
    pub control: u8,         // Control
    pub reserved: [u8; 3],   // Reserved
}

// Command header (32 bytes) - matches kernel/drivers/ahci/ahci.h
#[repr(C, packed)]
pub struct AhciCmdHeader {
    pub flags: u16,          // Command flags (CFL, ATAPI, Write, Prefetch, Reset)
    pub prdtl: u16,          // Physical region descriptor table length
    pub prdbc: u32,          // Physical region descriptor byte count
    pub ctba: u32,           // Command table base address (low)
    pub ctbau: u32,          // Command table base address (upper)
    pub reserved: [u32; 4],  // Reserved
}

// Command table (128 bytes + PRDT) - matches kernel/drivers/ahci/ahci.h
#[repr(C, packed)]
pub struct AhciCmdTable {
    pub cfis: [u8; 64],      // Command FIS (64 bytes)
    pub acmd: [u8; 16],      // ATAPI command (16 bytes)
    pub reserved: [u8; 48], // Reserved
    // PRDT follows (variable length)
}

// Physical region descriptor table entry (16 bytes) - matches kernel/drivers/ahci/ahci.h
#[repr(C, packed)]
pub struct AhciPrdtEntry {
    pub dba: u64,            // Data base address (64-bit)
    pub reserved: u32,       // Reserved
    pub dbc: u32,            // Data byte count (minus 1, bit 0 = interrupt)
}

