//! Block device interface via IPC

use crate::ipc::{send_block_read, send_block_write, BlockDeviceOp};

/// Read a single block from block device
pub fn block_read(device_id: u64, sector: u32, buffer: &mut [u8]) -> Result<(), ()> {
    send_block_read(device_id, sector, 1, buffer)
}

/// Write a single block to block device
pub fn block_write(device_id: u64, sector: u32, buffer: &[u8]) -> Result<(), ()> {
    send_block_write(device_id, sector, 1, buffer)
}

/// Read multiple blocks from block device
pub fn block_read_blocks(device_id: u64, sector: u32, count: u32, buffer: &mut [u8]) -> Result<(), ()> {
    send_block_read(device_id, sector, count, buffer)
}

/// Write multiple blocks to block device
pub fn block_write_blocks(device_id: u64, sector: u32, count: u32, buffer: &[u8]) -> Result<(), ()> {
    send_block_write(device_id, sector, count, buffer)
}

