//! ATA Storage Driver
//! 
//! User-space ATA driver for IDE/PATA storage devices.

#![no_std]
#![no_main]

use core::panic::PanicInfo;
use core::convert::TryInto;

extern crate alloc;
use alloc::vec::Vec;
use alloc::string::String;

use driver_framework::{Driver, DriverError, DeviceInfo, DeviceType};
use driver_framework::mmio::MmioRegion; // Not used for PIO, but generally useful
use driver_framework::interrupts;
use driver_framework::ipc::{ipc_create_port, ipc_receive, ipc_send, IpcMessage, IPC_MSG_REQUEST};
use driver_framework::syscalls;

use driver_framework::syscalls::{sys_sleep, sys_io_read, sys_io_write};

use crate::commands::{BLOCK_DEV_OP_READ, BLOCK_DEV_OP_WRITE}; // Assuming these are defined in a commands module

// ATA I/O Ports
const ATA_PRIMARY_BASE: u16 = 0x1F0;
const ATA_PRIMARY_CONTROL: u16 = 0x3F6;
const ATA_SECONDARY_BASE: u16 = 0x170;
const ATA_SECONDARY_CONTROL: u16 = 0x376;

// ATA Registers (offsets from base)
const ATA_DATA: u16 = 0x00;
const ATA_ERROR: u16 = 0x01;
const ATA_SECTOR_COUNT: u16 = 0x02;
const ATA_LBA_LOW: u16 = 0x03;
const ATA_LBA_MID: u16 = 0x04;
const ATA_LBA_HIGH: u16 = 0x05;
const ATA_DRIVE_SELECT: u16 = 0x06;
const ATA_STATUS: u16 = 0x07;
const ATA_COMMAND: u16 = 0x07;

// ATA Control Registers (offsets from control base)
const ATA_ALT_STATUS: u16 = 0x00;
const ATA_DEVICE_CONTROL: u16 = 0x00;

// ATA Commands
const ATA_CMD_READ_PIO: u8 = 0x20;
const ATA_CMD_READ_PIO_EXT: u8 = 0x24; // LBA48
const ATA_CMD_WRITE_PIO: u8 = 0x30;
const ATA_CMD_WRITE_PIO_EXT: u8 = 0x34; // LBA48
const ATA_CMD_IDENTIFY: u8 = 0xEC;

// ATA Status Register Bits
const ATA_SR_BSY: u8 = 0x80; // Busy
const ATA_SR_DRDY: u8 = 0x40; // Drive ready
const ATA_SR_DF: u8 = 0x20;  // Drive write fault
const ATA_SR_ERR: u8 = 0x01; // Error
const ATA_SR_DRQ: u8 = 0x08; // Data request ready

// ATA Drive Select Register Bits
const ATA_DRIVE_MASTER: u8 = 0xA0; // LBA mode, Master drive
const ATA_DRIVE_SLAVE: u8 = 0xB0;  // LBA mode, Slave drive

struct AtaChannel {
    base: u16,
    control: u16,
    drives: [Option<AtaDrive>; 2], // Master and Slave
}

#[derive(Clone)]
struct AtaDrive {
    channel_idx: u8,
    drive_idx: u8, // 0 for master, 1 for slave
    lba48: bool,
    sectors: u64,
    model: String,
    present: bool,
}

impl AtaDrive {
    fn new(channel_idx: u8, drive_idx: u8) -> Self {
        AtaDrive {
            channel_idx,
            drive_idx,
            lba48: false,
            sectors: 0,
            model: String::new(),
            present: false,
        }
    }
}

// Global driver instance
static mut DRIVER: AtaDriver = AtaDriver {
    initialized: false,
    device_port: 0,
    channels: [
        AtaChannel { base: ATA_PRIMARY_BASE, control: ATA_PRIMARY_CONTROL, drives: [None, None] },
        AtaChannel { base: ATA_SECONDARY_BASE, control: ATA_SECONDARY_CONTROL, drives: [None, None] },
    ],
};

#[no_mangle]
pub extern "C" fn _start() -> ! {
    ata_driver_init();
    ata_driver_loop();
}

fn ata_driver_init() {
    unsafe {
        DRIVER.device_port = ipc_create_port().expect("Failed to create ATA IPC port");
        
        // Register with driver manager
        driver_framework::driver_manager::register_driver(
            DRIVER.device_port,
            driver_framework::driver_manager::DriverType::Storage,
        ).expect("Failed to register ATA driver");

        // Initialize ATA channels and detect drives
        for ch_idx in 0..2 {
            let channel = &mut DRIVER.channels[ch_idx];
            
            // Software reset
            sys_io_write(channel.control, 0x02, 1); // Set SRST bit
            sys_io_write(channel.control, 0x00, 1); // Clear SRST bit
            syscalls::sys_sleep(10); // Wait for reset
            
            for dr_idx in 0..2 { // Master and Slave
                let drive_type_select = if dr_idx == 0 { ATA_DRIVE_MASTER } else { ATA_DRIVE_SLAVE };
                
                // Select drive
                sys_io_write(channel.base + ATA_DRIVE_SELECT, drive_type_select as u32, 1);
                sys_io_read(channel.base + ATA_STATUS, 1); // Read status to wait
                syscalls::sys_sleep(1);
                
                // Send IDENTIFY command
                sys_io_write(channel.base + ATA_COMMAND, ATA_CMD_IDENTIFY as u32, 1);
                
                // If drive exists, it will respond. Check status.
                let status = ata_read_status(channel);
                if status == 0 { continue; } // No drive
                
                ata_wait_bsy(channel);
                
                if (sys_io_read(channel.base + ATA_LBA_MID, 1) as u8 == 0 &&
                    sys_io_read(channel.base + ATA_LBA_HIGH, 1) as u8 == 0) {
                    // ATA device found
                    let mut drive = AtaDrive::new(ch_idx as u8, dr_idx as u8);
                    
                    // Read IDENTIFY data
                    ata_wait_drq(channel);
                    let mut data = [0u16; 256];
                    for i in 0..256 {
                        data[i] = sys_io_read(channel.base + ATA_DATA, 2) as u16;
                    }

                    // Parse IDENTIFY data
                    drive.lba48 = (data[83] & (1 << 10)) != 0;
                    if drive.lba48 {
                        drive.sectors = (data[100] as u64) | ((data[101] as u64) << 16) | ((data[102] as u64) << 32) | ((data[103] as u64) << 48);
                    } else {
                        drive.sectors = (data[60] as u64) | ((data[61] as u64) << 16);
                    }
                    
                    // Model string (words 27-46)
                    let model_bytes: [u8; 40] = core::mem::transmute(data[27..47]);
                    drive.model = String::from_utf8_lossy(&model_bytes)
                        .trim()
                        .to_string();

                    drive.present = true;
                    channel.drives[dr_idx] = Some(drive);
                }
            }
        }
        DRIVER.initialized = true;
    }
}

fn ata_driver_loop() -> ! {
    let mut msg = IpcMessage::new();
    loop {
        // Handle storage I/O requests via IPC
        if ipc_receive(DRIVER.device_port, &mut msg).is_ok() {
            let response = handle_ipc_message(&msg);
            let _ = ipc_send(msg.sender_tid, &response);
        }
        syscalls::sys_sleep(10); // Yield CPU
    }
}

fn handle_ipc_message(msg: &IpcMessage) -> IpcMessage {
    let mut response = IpcMessage::new();
    response.msg_type = driver_framework::ipc::IPC_MSG_RESPONSE;
    response.msg_id = msg.msg_id;

    match msg.msg_id {
        BLOCK_DEV_OP_READ => {
            if msg.inline_size >= 13 {
                let drive_idx = msg.inline_data[0] as usize; // Channel_idx:drive_idx (simplified for now)
                let lba = u64::from_le_bytes(msg.inline_data[1..9].try_into().unwrap());
                let count = u32::from_le_bytes(msg.inline_data[9..13].try_into().unwrap());
                
                if let Some(ref drive) = get_drive(drive_idx) {
                    let mut data_buffer = Vec::with_capacity((count * 512) as usize);
                    let res = ata_read_sectors_pio(drive, lba, count, &mut data_buffer);

                    if res.is_ok() {
                        // Copy data to response (limited to inline data for simplicity)
                        let copy_len = data_buffer.len().min(response.inline_data.len());
                        response.inline_data[0..copy_len].copy_from_slice(&data_buffer[0..copy_len]);
                        response.inline_size = copy_len as u32;
                    }
                }
            }
        }
        BLOCK_DEV_OP_WRITE => {
            if msg.inline_size >= 13 {
                let drive_idx = msg.inline_data[0] as usize;
                let lba = u64::from_le_bytes(msg.inline_data[1..9].try_into().unwrap());
                let count = u32::from_le_bytes(msg.inline_data[9..13].try_into().unwrap());
                
                if let Some(ref drive) = get_drive(drive_idx) {
                    let mut data_buffer = Vec::with_capacity((count * 512) as usize);
                    // Copy data from message inline data (limited for now)
                    let copy_len = data_buffer.capacity().min((msg.inline_size - 13) as usize);
                    data_buffer.extend_from_slice(&msg.inline_data[13..13 + copy_len]);

                    let res = ata_write_sectors_pio(drive, lba, count, &data_buffer);

                    if res.is_ok() {
                        response.inline_data[0] = 0; // Success
                        response.inline_size = 1;
                    }
                }
            }
        }
        _ => {}
    }

    response
}

fn get_drive(idx: usize) -> Option<&'static AtaDrive> {
    unsafe {
        if idx == 0 { return DRIVER.channels[0].drives[0].as_ref(); }
        if idx == 1 { return DRIVER.channels[0].drives[1].as_ref(); }
        if idx == 2 { return DRIVER.channels[1].drives[0].as_ref(); }
        if idx == 3 { return DRIVER.channels[1].drives[1].as_ref(); }
    }
    None
}

// ATA I/O Helpers
fn ata_read_status(channel: &AtaChannel) -> u8 {
    unsafe {
        sys_io_read(channel.base + ATA_STATUS, 1) as u8
    }
}

fn ata_wait_bsy(channel: &AtaChannel) {
    while ata_read_status(channel) & ATA_SR_BSY != 0 { syscalls::sys_yield(); }
}

fn ata_wait_drq(channel: &AtaChannel) {
    while ata_read_status(channel) & ATA_SR_DRQ == 0 { syscalls::sys_yield(); }
}

fn ata_select_drive(drive: &AtaDrive) {
    let channel = &unsafe { &mut DRIVER.channels[drive.channel_idx as usize] };
    let drive_select_val = if drive.drive_idx == 0 { ATA_DRIVE_MASTER } else { ATA_DRIVE_SLAVE };
    unsafe { sys_io_write(channel.base + ATA_DRIVE_SELECT, drive_select_val as u32, 1); }
    ata_read_status(channel); // Wait for drive select
}

fn ata_read_sectors_pio(drive: &AtaDrive, lba: u64, count: u32, buffer: &mut Vec<u8>) -> Result<(), ()> {
    let channel = &unsafe { &mut DRIVER.channels[drive.channel_idx as usize] };
    
    ata_wait_bsy(channel);
    ata_select_drive(drive);
    
    // Setup registers
    unsafe {
        sys_io_write(channel.base + ATA_SECTOR_COUNT, count as u32, 1);
        sys_io_write(channel.base + ATA_LBA_LOW, (lba & 0xFF) as u32, 1);
        sys_io_write(channel.base + ATA_LBA_MID, ((lba >> 8) & 0xFF) as u32, 1);
        sys_io_write(channel.base + ATA_LBA_HIGH, ((lba >> 16) & 0xFF) as u32, 1);
        
        let drive_select_val = if drive.drive_idx == 0 { ATA_DRIVE_MASTER } else { ATA_DRIVE_SLAVE };
        sys_io_write(channel.base + ATA_DRIVE_SELECT, (drive_select_val | ((lba >> 24) & 0x0F)) as u32, 1);
        
        sys_io_write(channel.base + ATA_COMMAND, ATA_CMD_READ_PIO as u32, 1);
    }
    
    for _ in 0..count {
        ata_wait_bsy(channel);
        ata_wait_drq(channel);
        
        unsafe {
            for _ in 0..256 { // 256 words per sector
                let word = sys_io_read(channel.base + ATA_DATA, 2) as u16;
                buffer.push((word & 0xFF) as u8);
                buffer.push(((word >> 8) & 0xFF) as u8);
            }
        }
    }
    Ok(())
}

fn ata_write_sectors_pio(drive: &AtaDrive, lba: u64, count: u32, data: &[u8]) -> Result<(), ()> {
    let channel = &unsafe { &mut DRIVER.channels[drive.channel_idx as usize] };
    
    ata_wait_bsy(channel);
    ata_select_drive(drive);
    
    // Setup registers
    unsafe {
        sys_io_write(channel.base + ATA_SECTOR_COUNT, count as u32, 1);
        sys_io_write(channel.base + ATA_LBA_LOW, (lba & 0xFF) as u32, 1);
        sys_io_write(channel.base + ATA_LBA_MID, ((lba >> 8) & 0xFF) as u32, 1);
        sys_io_write(channel.base + ATA_LBA_HIGH, ((lba >> 16) & 0xFF) as u32, 1);
        
        let drive_select_val = if drive.drive_idx == 0 { ATA_DRIVE_MASTER } else { ATA_DRIVE_SLAVE };
        sys_io_write(channel.base + ATA_DRIVE_SELECT, (drive_select_val | ((lba >> 24) & 0x0F)) as u32, 1);
        
        sys_io_write(channel.base + ATA_COMMAND, ATA_CMD_WRITE_PIO as u32, 1);
    }
    
    let mut data_offset = 0;
    for _ in 0..count {
        ata_wait_bsy(channel);
        ata_wait_drq(channel);
        
        unsafe {
            for _ in 0..256 { // 256 words per sector
                let word = u16::from_le_bytes([data[data_offset], data[data_offset + 1]]);
                sys_io_write(channel.base + ATA_DATA, word as u32, 2);
                data_offset += 2;
            }
        }
    }
    Ok(())
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}