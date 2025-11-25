#![no_std]
#![no_main]

//! Driver Manager Service
//!
//! Manages device drivers in user-space, providing:
//! - Driver registration and lifecycle management
//! - Device enumeration and discovery
//! - Request routing to appropriate drivers
//! - Driver crash recovery and restart

extern crate alloc;

use alloc::vec::Vec;
use alloc::string::String;
use core::panic::PanicInfo;

mod ipc;
use ipc::{IpcMessage, sys_ipc_receive, sys_ipc_send, sys_ipc_register_port};

// Driver Manager IPC port
const DRIVER_MANAGER_PORT: u32 = 100;

// Message types
const MSG_REGISTER_DRIVER: u32 = 1;
const MSG_UNREGISTER_DRIVER: u32 = 2;
const MSG_DEVICE_REQUEST: u32 = 3;
const MSG_ENUMERATE_DEVICES: u32 = 4;
const MSG_DRIVER_CRASHED: u32 = 5;

// Driver types
#[derive(Clone, Copy, PartialEq)]
#[repr(u32)]
enum DriverType {
    PciBus = 1,
    Storage = 2,
    Network = 3,
    Input = 4,
    Graphics = 5,
    Audio = 6,
    Unknown = 0xFF,
}

// Driver state
#[derive(Clone, Copy, PartialEq)]
enum DriverState {
    Registered,
    Running,
    Crashed,
    Stopped,
}

// Registered driver information
struct RegisteredDriver {
    driver_id: u32,
    driver_type: DriverType,
    driver_port: u32,
    driver_pid: u32,
    state: DriverState,
    crash_count: u32,
}

// Device information
struct Device {
    device_id: u32,
    driver_id: u32,
    device_type: DriverType,
    vendor_id: u16,
    device_id_hw: u16,
}

// Driver Manager state
struct DriverManager {
    drivers: Vec<RegisteredDriver>,
    devices: Vec<Device>,
    next_driver_id: u32,
    next_device_id: u32,
}

impl DriverManager {
    fn new() -> Self {
        DriverManager {
            drivers: Vec::new(),
            devices: Vec::new(),
            next_driver_id: 1,
            next_device_id: 1,
        }
    }

    fn register_driver(&mut self, driver_type: DriverType, driver_port: u32, driver_pid: u32) -> u32 {
        let driver_id = self.next_driver_id;
        self.next_driver_id += 1;

        let driver = RegisteredDriver {
            driver_id,
            driver_type,
            driver_port,
            driver_pid,
            state: DriverState::Registered,
            crash_count: 0,
        };

        self.drivers.push(driver);
        driver_id
    }

    fn unregister_driver(&mut self, driver_id: u32) -> bool {
        if let Some(pos) = self.drivers.iter().position(|d| d.driver_id == driver_id) {
            self.drivers.remove(pos);
            // Remove all devices associated with this driver
            self.devices.retain(|dev| dev.driver_id != driver_id);
            true
        } else {
            false
        }
    }

    fn find_driver_by_type(&self, driver_type: DriverType) -> Option<&RegisteredDriver> {
        self.drivers.iter()
            .find(|d| d.driver_type == driver_type && d.state == DriverState::Running)
    }

    fn find_driver_by_id(&self, driver_id: u32) -> Option<&RegisteredDriver> {
        self.drivers.iter().find(|d| d.driver_id == driver_id)
    }

    fn find_driver_by_id_mut(&mut self, driver_id: u32) -> Option<&mut RegisteredDriver> {
        self.drivers.iter_mut().find(|d| d.driver_id == driver_id)
    }

    fn register_device(&mut self, driver_id: u32, device_type: DriverType, vendor_id: u16, device_id_hw: u16) -> u32 {
        let device_id = self.next_device_id;
        self.next_device_id += 1;

        let device = Device {
            device_id,
            driver_id,
            device_type,
            vendor_id,
            device_id_hw,
        };

        self.devices.push(device);
        device_id
    }

    fn handle_driver_crash(&mut self, driver_id: u32) {
        if let Some(driver) = self.find_driver_by_id_mut(driver_id) {
            driver.state = DriverState::Crashed;
            driver.crash_count += 1;

            // Auto-restart if crash count is below threshold
            if driver.crash_count < 3 {
                // Send restart request to process manager
                // Assuming process manager port is 101 and message ID 1
                let mut restart_msg = IpcMessage::new();
                restart_msg.msg_type = ipc::IPC_MSG_REQUEST;
                restart_msg.msg_id = 1; // PM_MSG_RESTART_PROCESS
                restart_msg.inline_data[0..4].copy_from_slice(&driver.driver_pid.to_le_bytes()); // PID to restart
                restart_msg.inline_size = 4;
                let _ = sys_ipc_send(101, &restart_msg); // Assuming 101 is Process Manager's port
                driver.state = DriverState::Registered; // Mark as registered for restart
            }
        }
    }

    fn enumerate_devices(&self, device_type: DriverType) -> Vec<u32> {
        self.devices.iter()
            .filter(|dev| dev.device_type == device_type)
            .map(|dev| dev.device_id)
            .collect()
    }
}

static mut DRIVER_MANAGER: Option<DriverManager> = None;

#[no_mangle]
pub extern "C" fn _start() -> ! {
    driver_manager_init();
    driver_manager_loop();
}

fn driver_manager_init() {
    unsafe {
        DRIVER_MANAGER = Some(DriverManager::new());
    }

    // Register our IPC port
    if sys_ipc_register_port(DRIVER_MANAGER_PORT) != 0 {
        // Failed to register port - panic
        loop {}
    }
}

fn driver_manager_loop() -> ! {
    let mut msg = IpcMessage::new();

    loop {
        // Receive IPC message
        if sys_ipc_receive(DRIVER_MANAGER_PORT, &mut msg) == 0 {
            let response = handle_message(&msg);
            
            // Send response back to sender
            let _ = sys_ipc_send(msg.sender_tid, &response);
        }
    }
}

fn handle_message(msg: &IpcMessage) -> IpcMessage {
    let mut response = IpcMessage::new();
    response.msg_type = ipc::IPC_MSG_RESPONSE;
    response.msg_id = msg.msg_id;

    unsafe {
        if let Some(ref mut manager) = DRIVER_MANAGER {
            match msg.msg_id {
                MSG_REGISTER_DRIVER => {
                    // Extract driver type, port, and PID from message
                    let driver_type = match msg.inline_data[0] as u32 {
                        1 => DriverType::PciBus,
                        2 => DriverType::Storage,
                        3 => DriverType::Network,
                        4 => DriverType::Input,
                        5 => DriverType::Graphics,
                        6 => DriverType::Audio,
                        _ => DriverType::Unknown,
                    };
                    let driver_port = u32::from_le_bytes([
                        msg.inline_data[1],
                        msg.inline_data[2],
                        msg.inline_data[3],
                        msg.inline_data[4],
                    ]);
                    let driver_pid = msg.sender_tid;

                    let driver_id = manager.register_driver(driver_type, driver_port, driver_pid);

                    // Return driver ID
                    response.inline_data[0..4].copy_from_slice(&driver_id.to_le_bytes());
                    response.inline_size = 4;
                }

                MSG_UNREGISTER_DRIVER => {
                    let driver_id = u32::from_le_bytes([
                        msg.inline_data[0],
                        msg.inline_data[1],
                        msg.inline_data[2],
                        msg.inline_data[3],
                    ]);

                    let success = manager.unregister_driver(driver_id);
                    response.inline_data[0] = if success { 1 } else { 0 };
                    response.inline_size = 1;
                }

                MSG_DEVICE_REQUEST => {
                    // Route request to appropriate driver
                    let device_type = match msg.inline_data[0] as u32 {
                        1 => DriverType::PciBus,
                        2 => DriverType::Storage,
                        3 => DriverType::Network,
                        4 => DriverType::Input,
                        _ => DriverType::Unknown,
                    };

                    if let Some(driver) = manager.find_driver_by_type(device_type) {
                        // Forward request to driver
                        let mut fwd_msg = *msg;
                        if sys_ipc_send(driver.driver_port, &fwd_msg) == 0 {
                            // Wait for driver response
                            if sys_ipc_receive(DRIVER_MANAGER_PORT, &mut response) == 0 {
                                // Return driver response
                            } else {
                                response.inline_data[0] = 0xFF; // Error
                                response.inline_size = 1;
                            }
                        } else {
                            response.inline_data[0] = 0xFE; // Forward failed
                            response.inline_size = 1;
                        }
                    } else {
                        response.inline_data[0] = 0xFD; // No driver found
                        response.inline_size = 1;
                    }
                }

                MSG_ENUMERATE_DEVICES => {
                    let device_type = match msg.inline_data[0] as u32 {
                        1 => DriverType::PciBus,
                        2 => DriverType::Storage,
                        3 => DriverType::Network,
                        4 => DriverType::Input,
                        _ => DriverType::Unknown,
                    };

                    let devices = manager.enumerate_devices(device_type);
                    let count = devices.len().min(16); // Max 16 devices in response

                    response.inline_data[0] = count as u8;
                    for (i, &device_id) in devices.iter().take(count).enumerate() {
                        let offset = 1 + i * 4;
                        response.inline_data[offset..offset + 4].copy_from_slice(&device_id.to_le_bytes());
                    }
                    response.inline_size = 1 + (count * 4) as u32;
                }

                MSG_DRIVER_CRASHED => {
                    let driver_id = u32::from_le_bytes([
                        msg.inline_data[0],
                        msg.inline_data[1],
                        msg.inline_data[2],
                        msg.inline_data[3],
                    ]);

                    manager.handle_driver_crash(driver_id);
                    response.inline_data[0] = 1; // Acknowledged
                    response.inline_size = 1;
                }

                _ => {
                    // Unknown message type
                    response.inline_data[0] = 0xFF;
                    response.inline_size = 1;
                }
            }
        } else {
            // Manager not initialized
            response.inline_data[0] = 0xFC;
            response.inline_size = 1;
        }
    }

    response
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}
