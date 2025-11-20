//! User-Space Driver Framework
//! 
//! This framework provides the infrastructure for user-space drivers
//! to communicate with the kernel and device manager service.

#![no_std]

pub mod ipc;
pub mod syscalls;
pub mod mmio;
pub mod dma;
pub mod interrupts;

// Re-export commonly used items
pub use ipc::{IpcMessage, IPC_MSG_REQUEST, IPC_MSG_RESPONSE};

/// Driver trait that all user-space drivers must implement
pub trait Driver {
    /// Initialize the driver
    fn init(&mut self) -> Result<(), DriverError>;
    
    /// Probe for device compatibility
    fn probe(&self, device_info: &DeviceInfo) -> bool;
    
    /// Start the driver
    fn start(&mut self) -> Result<(), DriverError>;
    
    /// Stop the driver
    fn stop(&mut self) -> Result<(), DriverError>;
    
    /// Get driver name
    fn name(&self) -> &'static str;
    
    /// Get driver version
    fn version(&self) -> &'static str;
}

/// Device information passed to drivers
#[repr(C)]
pub struct DeviceInfo {
    pub device_type: DeviceType,
    pub vendor_id: u16,
    pub device_id: u16,
    pub class_code: u8,
    pub subclass: u8,
    pub interface: u8,
    pub bus: u8,
    pub device: u8,
    pub function: u8,
    pub bars: [u64; 6],
    pub irq_line: u8,
    pub irq_pin: u8,
}

#[repr(u32)]
pub enum DeviceType {
    Pci = 1,
    Usb = 2,
    I2c = 3,
    Spi = 4,
    Serial = 5,
}

/// Driver errors
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DriverError {
    InvalidArgument,
    DeviceNotFound,
    NotSupported,
    OutOfMemory,
    IoError,
    Timeout,
    AlreadyInitialized,
    NotInitialized,
    PermissionDenied,
    Unknown,
}

impl From<u64> for DriverError {
    fn from(code: u64) -> Self {
        match code {
            1 => DriverError::InvalidArgument,
            2 => DriverError::DeviceNotFound,
            3 => DriverError::NotSupported,
            4 => DriverError::OutOfMemory,
            5 => DriverError::IoError,
            6 => DriverError::Timeout,
            7 => DriverError::AlreadyInitialized,
            8 => DriverError::NotInitialized,
            9 => DriverError::PermissionDenied,
            _ => DriverError::Unknown,
        }
    }
}

impl Into<u64> for DriverError {
    fn into(self) -> u64 {
        match self {
            DriverError::InvalidArgument => 1,
            DriverError::DeviceNotFound => 2,
            DriverError::NotSupported => 3,
            DriverError::OutOfMemory => 4,
            DriverError::IoError => 5,
            DriverError::Timeout => 6,
            DriverError::AlreadyInitialized => 7,
            DriverError::NotInitialized => 8,
            DriverError::PermissionDenied => 9,
            DriverError::Unknown => 255,
        }
    }
}

