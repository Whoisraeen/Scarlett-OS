/**
 * @file ata_driver.rs
 * @brief User-space ATA/IDE driver
 */

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

// IPC syscall wrappers
extern "C" {
    fn sys_ipc_send(tid: u32, msg: *const IpcMessage) -> i32;
    fn sys_ipc_receive(port: u32, msg: *mut IpcMessage) -> i32;
    fn sys_ipc_register_port(port: u32) -> i32;
    fn sys_io_read(port: u16, size: u8) -> u32;
    fn sys_io_write(port: u16, value: u32, size: u8) -> i32;
}

#[repr(C)]
struct IpcMessage {
    sender_tid: u32,
    msg_type: u32,
    data: [u8; 256],
}

// ATA Driver IPC port
const ATA_DRIVER_PORT: u32 = 102;

// ATA I/O ports
const ATA_PRIMARY_IO: u16 = 0x1F0;
const ATA_PRIMARY_CTRL: u16 = 0x3F6;
const ATA_SECONDARY_IO: u16 = 0x170;
const ATA_SECONDARY_CTRL: u16 = 0x376;

// ATA registers
const ATA_REG_DATA: u16 = 0;
const ATA_REG_ERROR: u16 = 1;
const ATA_REG_FEATURES: u16 = 1;
const ATA_REG_SECCOUNT: u16 = 2;
const ATA_REG_LBA_LO: u16 = 3;
const ATA_REG_LBA_MID: u16 = 4;
const ATA_REG_LBA_HI: u16 = 5;
const ATA_REG_DRIVE: u16 = 6;
const ATA_REG_STATUS: u16 = 7;
const ATA_REG_COMMAND: u16 = 7;

// ATA commands
const ATA_CMD_READ_SECTORS: u8 = 0x20;
const ATA_CMD_WRITE_SECTORS: u8 = 0x30;
const ATA_CMD_IDENTIFY: u8 = 0xEC;

// Message types
const MSG_ATA_READ: u32 = 1;
const MSG_ATA_WRITE: u32 = 2;
const MSG_ATA_IDENTIFY: u32 = 3;

#[repr(C)]
struct AtaDevice {
    base_io: u16,
    ctrl_io: u16,
    is_master: bool,
    exists: bool,
    sectors: u64,
}

static mut PRIMARY_MASTER: AtaDevice = AtaDevice {
    base_io: ATA_PRIMARY_IO,
    ctrl_io: ATA_PRIMARY_CTRL,
    is_master: true,
    exists: false,
    sectors: 0,
};

static mut PRIMARY_SLAVE: AtaDevice = AtaDevice {
    base_io: ATA_PRIMARY_IO,
    ctrl_io: ATA_PRIMARY_CTRL,
    is_master: false,
    exists: false,
    sectors: 0,
};

#[no_mangle]
pub extern "C" fn _start() -> ! {
    // Register with driver manager
    register_with_driver_manager();

    // Detect ATA devices
    detect_devices();

    // Register IPC port
    unsafe {
        sys_ipc_register_port(ATA_DRIVER_PORT);
    }

    // Main service loop
    loop {
        let mut msg = IpcMessage {
            sender_tid: 0,
            msg_type: 0,
            data: [0; 256],
        };

        unsafe {
            if sys_ipc_receive(ATA_DRIVER_PORT, &mut msg) == 0 {
                let response = handle_message(&msg);
                let _ = sys_ipc_send(msg.sender_tid, &response);
            }
        }
    }
}

fn register_with_driver_manager() {
    const DRIVER_MANAGER_PORT: u32 = 100;
    const MSG_REGISTER_DRIVER: u32 = 1;
    const DRIVER_TYPE_STORAGE: u32 = 2;
    
    // Create registration message
    let mut msg = IpcMessage {
        sender_tid: 0,  // Will be filled by kernel
        msg_type: 1,    // IPC_MSG_REQUEST
        data: [0; 256],
    };
    
    // Pack driver type (Storage = 2) and port into message data
    msg.data[0] = DRIVER_TYPE_STORAGE as u8;
    msg.data[1..5].copy_from_slice(&ATA_DRIVER_PORT.to_le_bytes());
    
    // Send registration message to driver manager
    unsafe {
        let _ = sys_ipc_send(DRIVER_MANAGER_PORT, &msg);
    }
}

fn detect_devices() {
    unsafe {
        // Try to identify primary master
        if identify_device(&mut PRIMARY_MASTER) {
            PRIMARY_MASTER.exists = true;
        }

        // Try to identify primary slave
        if identify_device(&mut PRIMARY_SLAVE) {
            PRIMARY_SLAVE.exists = true;
        }
    }
}

fn identify_device(device: &mut AtaDevice) -> bool {
    unsafe {
        // Select drive
        let drive_select = if device.is_master { 0xA0 } else { 0xB0 };
        sys_io_write(device.base_io + ATA_REG_DRIVE, drive_select as u32, 1);

        // Wait for drive to be ready
        ata_wait_ready(device.base_io);

        // Send IDENTIFY command
        sys_io_write(device.base_io + ATA_REG_COMMAND, ATA_CMD_IDENTIFY as u32, 1);

        // Wait for response
        let status = sys_io_read(device.base_io + ATA_REG_STATUS, 1) as u8;
        if status == 0 {
            return false; // No device
        }

        // Wait for data ready
        ata_wait_drq(device.base_io);

        // Read identification data
        let mut id_data = [0u16; 256];
        for i in 0..256 {
            id_data[i] = sys_io_read(device.base_io + ATA_REG_DATA, 2) as u16;
        }

        // Extract sector count (words 60-61)
        device.sectors = ((id_data[61] as u64) << 16) | (id_data[60] as u64);

        true
    }
}

fn ata_wait_ready(base_io: u16) {
    unsafe {
        loop {
            let status = sys_io_read(base_io + ATA_REG_STATUS, 1) as u8;
            if (status & 0x80) == 0 {
                // BSY clear
                break;
            }
        }
    }
}

fn ata_wait_drq(base_io: u16) {
    unsafe {
        loop {
            let status = sys_io_read(base_io + ATA_REG_STATUS, 1) as u8;
            if (status & 0x08) != 0 {
                // DRQ set
                break;
            }
        }
    }
}

fn handle_message(msg: &IpcMessage) -> IpcMessage {
    match msg.msg_type {
        MSG_ATA_READ => handle_read(msg),
        MSG_ATA_WRITE => handle_write(msg),
        MSG_ATA_IDENTIFY => handle_identify(msg),
        _ => create_error_response(1),
    }
}

fn handle_read(msg: &IpcMessage) -> IpcMessage {
    let lba = u32::from_le_bytes([msg.data[0], msg.data[1], msg.data[2], msg.data[3]]);
    let count = msg.data[4];

    unsafe {
        // Use primary master for now
        if !PRIMARY_MASTER.exists {
            return create_error_response(2);
        }

        read_sectors(&PRIMARY_MASTER, lba, count, &mut [0u8; 512])
    }
}

fn handle_write(_msg: &IpcMessage) -> IpcMessage {
    // TODO: Implement write
    create_success_response()
}

fn handle_identify(_msg: &IpcMessage) -> IpcMessage {
    // TODO: Return device info
    create_success_response()
}

fn read_sectors(device: &AtaDevice, lba: u32, count: u8, buffer: &mut [u8]) -> IpcMessage {
    unsafe {
        // Select drive
        let drive_select = if device.is_master { 0xE0 } else { 0xF0 };
        sys_io_write(device.base_io + ATA_REG_DRIVE, (drive_select | ((lba >> 24) & 0x0F)) as u32, 1);

        // Set sector count
        sys_io_write(device.base_io + ATA_REG_SECCOUNT, count as u32, 1);

        // Set LBA
        sys_io_write(device.base_io + ATA_REG_LBA_LO, (lba & 0xFF) as u32, 1);
        sys_io_write(device.base_io + ATA_REG_LBA_MID, ((lba >> 8) & 0xFF) as u32, 1);
        sys_io_write(device.base_io + ATA_REG_LBA_HI, ((lba >> 16) & 0xFF) as u32, 1);

        // Send read command
        sys_io_write(device.base_io + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS as u32, 1);

        // Wait for data
        ata_wait_drq(device.base_io);

        // Read data
        for i in 0..256 {
            let word = sys_io_read(device.base_io + ATA_REG_DATA, 2) as u16;
            if i * 2 < buffer.len() {
                buffer[i * 2] = (word & 0xFF) as u8;
                buffer[i * 2 + 1] = (word >> 8) as u8;
            }
        }

        create_success_response()
    }
}

fn create_success_response() -> IpcMessage {
    IpcMessage {
        sender_tid: 0,
        msg_type: 0,
        data: [0; 256],
    }
}

fn create_error_response(error_code: u32) -> IpcMessage {
    let mut response = IpcMessage {
        sender_tid: 0,
        msg_type: 1,
        data: [0; 256],
    };
    response.data[0..4].copy_from_slice(&error_code.to_le_bytes());
    response
}
