//! FAT32 filesystem implementation

use core::mem;

/// FAT32 Boot Sector structure (must match kernel/include/fs/fat32.h)
#[repr(C, packed)]
pub struct Fat32BootSector {
    pub jump: [u8; 3],
    pub oem_name: [u8; 8],
    pub bytes_per_sector: u16,
    pub sectors_per_cluster: u8,
    pub reserved_sectors: u16,
    pub num_fats: u8,
    pub root_entries: u16,
    pub total_sectors_16: u16,
    pub media_type: u8,
    pub sectors_per_fat_16: u16,
    pub sectors_per_track: u16,
    pub num_heads: u16,
    pub hidden_sectors: u32,
    pub total_sectors_32: u32,
    pub sectors_per_fat_32: u32,
    pub flags: u16,
    pub version: u16,
    pub root_cluster: u32,
    pub fs_info_sector: u16,
    pub backup_boot_sector: u16,
    pub reserved: [u8; 12],
    pub drive_number: u8,
    pub reserved1: u8,
    pub boot_signature: u8,
    pub volume_id: u32,
    pub volume_label: [u8; 11],
    pub fs_type: [u8; 8],
    pub boot_code: [u8; 420],
    pub boot_signature_end: u16,
}

/// FAT32 Directory Entry
#[repr(C, packed)]
pub struct Fat32DirEntry {
    pub name: [u8; 11],
    pub attributes: u8,
    pub reserved: u8,
    pub creation_time_tenths: u8,
    pub creation_time: u16,
    pub creation_date: u16,
    pub access_date: u16,
    pub cluster_high: u16,
    pub modification_time: u16,
    pub modification_date: u16,
    pub cluster_low: u16,
    pub file_size: u32,
}

/// FAT32 filesystem structure
pub struct Fat32Fs {
    pub device_id: u64,  // Block device ID
    pub boot_sector: Fat32BootSector,
    pub sectors_per_cluster: u32,
    pub bytes_per_cluster: u32,
    pub fat_start_sector: u32,
    pub fat_size_sectors: u32,
    pub data_start_sector: u32,
    pub root_cluster: u32,
    pub total_clusters: u32,
    pub fat_cache: [u8; 512],
    pub fat_cache_sector: u32,
}

/// FAT32 cluster values
pub const FAT32_CLUSTER_FREE: u32 = 0x00000000;
pub const FAT32_CLUSTER_EOF_MIN: u32 = 0x0FFFFFF8;
pub const FAT32_CLUSTER_EOF_MAX: u32 = 0x0FFFFFFF;

/// Initialize FAT32 filesystem
pub fn fat32_init(device_id: u64, fs: &mut Fat32Fs) -> Result<(), ()> {
    // Read boot sector
    let mut boot_sector = Fat32BootSector {
        jump: [0; 3],
        oem_name: [0; 8],
        bytes_per_sector: 0,
        sectors_per_cluster: 0,
        reserved_sectors: 0,
        num_fats: 0,
        root_entries: 0,
        total_sectors_16: 0,
        media_type: 0,
        sectors_per_fat_16: 0,
        sectors_per_track: 0,
        num_heads: 0,
        hidden_sectors: 0,
        total_sectors_32: 0,
        sectors_per_fat_32: 0,
        flags: 0,
        version: 0,
        root_cluster: 0,
        fs_info_sector: 0,
        backup_boot_sector: 0,
        reserved: [0; 12],
        drive_number: 0,
        reserved1: 0,
        boot_signature: 0,
        volume_id: 0,
        volume_label: [0; 11],
        fs_type: [0; 8],
        boot_code: [0; 420],
        boot_signature_end: 0,
    };
    
    // Read boot sector via block device
    if block_read(device_id, 0, unsafe {
        core::slice::from_raw_parts_mut(
            &mut boot_sector as *mut _ as *mut u8,
            core::mem::size_of::<Fat32BootSector>()
        )
    }).is_err() {
        return Err(());
    }
    
    // Verify boot signature
    if boot_sector.boot_signature_end.to_le() != 0xAA55 {
        return Err(());
    }
    
    // Verify FS type
    if &boot_sector.fs_type[0..5] != b"FAT32" {
        return Err(());
    }
    
    // Calculate filesystem parameters
    fs.device_id = device_id;
    fs.boot_sector = boot_sector;
    fs.sectors_per_cluster = fs.boot_sector.sectors_per_cluster as u32;
    fs.bytes_per_cluster = fs.sectors_per_cluster * fs.boot_sector.bytes_per_sector as u32;
    fs.fat_start_sector = fs.boot_sector.reserved_sectors as u32;
    fs.fat_size_sectors = fs.boot_sector.sectors_per_fat_32;
    fs.data_start_sector = fs.fat_start_sector + (fs.boot_sector.num_fats as u32 * fs.fat_size_sectors);
    fs.root_cluster = fs.boot_sector.root_cluster;
    
    // Calculate total clusters
    let data_sectors = fs.boot_sector.total_sectors_32 - fs.data_start_sector;
    fs.total_clusters = data_sectors / fs.sectors_per_cluster;
    
    fs.fat_cache_sector = 0xFFFFFFFF;  // Invalid
    
    Ok(())
}

/// Read a cluster from the filesystem
pub fn fat32_read_cluster(fs: &Fat32Fs, cluster: u32, buffer: &mut [u8]) -> Result<(), ()> {
    if cluster < 2 || cluster >= fs.total_clusters + 2 {
        return Err(());
    }
    
    // Calculate sector number
    let first_sector = fs.data_start_sector + ((cluster - 2) * fs.sectors_per_cluster);
    
    // Read cluster
    block_read_blocks(fs.device_id, first_sector, fs.sectors_per_cluster, buffer)
}

/// Get next cluster in chain
pub fn fat32_get_next_cluster(fs: &mut Fat32Fs, cluster: u32) -> u32 {
    if cluster < 2 || cluster >= fs.total_clusters + 2 {
        return FAT32_CLUSTER_EOF_MIN;
    }
    
    // Calculate FAT entry location
    let fat_offset = cluster * 4;  // 4 bytes per FAT32 entry
    let fat_sector = fs.fat_start_sector + (fat_offset / fs.boot_sector.bytes_per_sector as u32);
    let fat_entry_offset = (fat_offset % fs.boot_sector.bytes_per_sector as u32) as usize;
    
    // Read FAT sector if not cached
    if fat_sector != fs.fat_cache_sector {
        if block_read(fs.device_id, fat_sector, &mut fs.fat_cache).is_err() {
            return FAT32_CLUSTER_EOF_MIN;
        }
        fs.fat_cache_sector = fat_sector;
    }
    
    // Read FAT entry
    let fat_entry = u32::from_le_bytes([
        fs.fat_cache[fat_entry_offset],
        fs.fat_cache[fat_entry_offset + 1],
        fs.fat_cache[fat_entry_offset + 2],
        fs.fat_cache[fat_entry_offset + 3],
    ]);
    
    fat_entry & 0x0FFFFFFF  // Mask upper 4 bits
}

