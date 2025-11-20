//! Mock driver infrastructure for testing

#![no_std]

use core::sync::atomic::{AtomicU64, Ordering};

/// Mock AHCI driver
pub struct MockAhciDriver {
    port: u64,
}

static MOCK_AHCI_PORT: AtomicU64 = AtomicU64::new(0);

impl MockAhciDriver {
    pub fn new() -> Self {
        Self { port: 0 }
    }
    
    pub fn init(&mut self) -> Result<(), ()> {
        // In real implementation, would create IPC port
        // For mock, use a fixed port ID
        self.port = 10;
        MOCK_AHCI_PORT.store(self.port, Ordering::Relaxed);
        Ok(())
    }
    
    pub fn get_port(&self) -> u64 {
        self.port
    }
    
    pub fn handle_read_request(&self, _lba: u64, count: u32) -> Result<usize, ()> {
        // Mock: return size of data that would be read
        // In real implementation, would copy data to provided buffer
        Ok((count as usize) * 512)
    }
}

/// Mock Ethernet driver
pub struct MockEthernetDriver {
    port: u64,
}

static MOCK_ETHERNET_PORT: AtomicU64 = AtomicU64::new(0);

impl MockEthernetDriver {
    pub fn new() -> Self {
        Self { port: 0 }
    }
    
    pub fn init(&mut self) -> Result<(), ()> {
        // In real implementation, would create IPC port
        // For mock, use a fixed port ID
        self.port = 11;
        MOCK_ETHERNET_PORT.store(self.port, Ordering::Relaxed);
        Ok(())
    }
    
    pub fn get_port(&self) -> u64 {
        self.port
    }
    
    pub fn handle_send_request(&self, _data: &[u8]) -> Result<(), ()> {
        // Mock: always succeed
        Ok(())
    }
}

