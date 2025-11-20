//! Mock service infrastructure for testing

#![no_std]

use core::sync::atomic::{AtomicU64, Ordering};

/// Mock VFS service
pub struct MockVfsService {
    port: u64,
    block_device_port: u64,
}

static MOCK_VFS_PORT: AtomicU64 = AtomicU64::new(0);

impl MockVfsService {
    pub fn new() -> Self {
        Self {
            port: 0,
            block_device_port: 0,
        }
    }
    
    pub fn init(&mut self) -> Result<(), ()> {
        // In real implementation, would create IPC port
        // For mock, use a fixed port ID
        self.port = 2;
        MOCK_VFS_PORT.store(self.port, Ordering::Relaxed);
        Ok(())
    }
    
    pub fn get_port(&self) -> u64 {
        self.port
    }
    
    pub fn set_block_device_port(&mut self, port: u64) {
        self.block_device_port = port;
    }
    
    pub fn get_block_device_port(&self) -> u64 {
        self.block_device_port
    }
}

/// Mock Network service
pub struct MockNetworkService {
    port: u64,
    ethernet_device_port: u64,
}

static MOCK_NETWORK_PORT: AtomicU64 = AtomicU64::new(0);

impl MockNetworkService {
    pub fn new() -> Self {
        Self {
            port: 0,
            ethernet_device_port: 0,
        }
    }
    
    pub fn init(&mut self) -> Result<(), ()> {
        // In real implementation, would create IPC port
        // For mock, use a fixed port ID
        self.port = 3;
        MOCK_NETWORK_PORT.store(self.port, Ordering::Relaxed);
        Ok(())
    }
    
    pub fn get_port(&self) -> u64 {
        self.port
    }
    
    pub fn set_ethernet_device_port(&mut self, port: u64) {
        self.ethernet_device_port = port;
    }
    
    pub fn get_ethernet_device_port(&self) -> u64 {
        self.ethernet_device_port
    }
}

