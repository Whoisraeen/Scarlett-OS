//! User-Space Ethernet Driver (E1000)
//! 
//! This driver implements support for Intel E1000 network cards.

#![no_std]
#![no_main]

mod packet;

use driver_framework::{Driver, DriverError, DeviceInfo, DeviceType};
use driver_framework::mmio::MmioRegion;
use driver_framework::interrupts;
use driver_framework::ipc::{ipc_create_port, ipc_receive, ipc_send, IpcMessage};
use driver_framework::dma::DmaBuffer;
use packet::{NET_DEV_OP_SEND, NET_DEV_OP_RECEIVE, NET_DEV_OP_GET_MAC, NET_DEV_OP_SET_IP};

// E1000 Registers
const E1000_CTRL: usize = 0x0000;
const E1000_STATUS: usize = 0x0008;
const E1000_EEPROM: usize = 0x0014;
const E1000_ICR: usize = 0x00C0;
const E1000_IMS: usize = 0x00D0;
const E1000_RCTL: usize = 0x0100;
const E1000_TCTL: usize = 0x0400;
const E1000_RDBAL: usize = 0x2800;
const E1000_RDBAH: usize = 0x2804;
const E1000_RDLEN: usize = 0x2808;
const E1000_RDH: usize = 0x2810;
const E1000_RDT: usize = 0x2818;
const E1000_TDBAL: usize = 0x3800;
const E1000_TDBAH: usize = 0x3804;
const E1000_TDLEN: usize = 0x3808;
const E1000_TDH: usize = 0x3810;
const E1000_TDT: usize = 0x3818;
const E1000_MTA: usize = 0x5200;

// Constants
const RX_DESC_COUNT: usize = 32;
const TX_DESC_COUNT: usize = 32;
const E1000_RCTL_EN: u32 = 1 << 1;
const E1000_RCTL_SBP: u32 = 1 << 2;
const E1000_RCTL_UPE: u32 = 1 << 3;
const E1000_RCTL_MPE: u32 = 1 << 4;
const E1000_RCTL_LPE: u32 = 1 << 5;
const E1000_RCTL_BAM: u32 = 1 << 15;
const E1000_RCTL_SECRC: u32 = 1 << 26;
const E1000_TCTL_EN: u32 = 1 << 1;
const E1000_TCTL_PSP: u32 = 1 << 3;
const E1000_CMD_EOP: u8 = 1 << 0;
const E1000_CMD_IFCS: u8 = 1 << 1;
const E1000_CMD_RS: u8 = 1 << 3;

#[repr(C, packed)]
struct RxDesc {
    addr: u64,
    length: u16,
    checksum: u16,
    status: u8,
    errors: u8,
    special: u16,
}

#[repr(C, packed)]
struct TxDesc {
    addr: u64,
    length: u16,
    cso: u8,
    cmd: u8,
    status: u8,
    css: u8,
    special: u16,
}

struct EthernetDriver {
    initialized: bool,
    device_port: u64,
    mmio: Option<MmioRegion>,
    mac_address: [u8; 6],
    irq: u8,
    
    // E1000 specific
    rx_desc_ring: Option<DmaBuffer>,
    tx_desc_ring: Option<DmaBuffer>,
    rx_buffers: Option<DmaBuffer>, // One large buffer for all RX packets
    tx_buffers: Option<DmaBuffer>, // One large buffer for all TX packets
    rx_cur: usize,
    tx_cur: usize,
}

impl EthernetDriver {
    fn new() -> Self {
        Self {
            initialized: false,
            device_port: 0,
            mmio: None,
            mac_address: [0; 6],
            irq: 0,
            rx_desc_ring: None,
            tx_desc_ring: None,
            rx_buffers: None,
            tx_buffers: None,
            rx_cur: 0,
            tx_cur: 0,
        }
    }
    
    fn read_mac(&mut self) {
        if let Some(ref mmio) = self.mmio {
            let low = unsafe { mmio.read_u32(0x5400) }; // RAL
            let high = unsafe { mmio.read_u32(0x5404) }; // RAH
            
            if low != 0 || high != 0 {
                self.mac_address[0] = (low & 0xFF) as u8;
                self.mac_address[1] = ((low >> 8) & 0xFF) as u8;
                self.mac_address[2] = ((low >> 16) & 0xFF) as u8;
                self.mac_address[3] = ((low >> 24) & 0xFF) as u8;
                self.mac_address[4] = (high & 0xFF) as u8;
                self.mac_address[5] = ((high >> 8) & 0xFF) as u8;
            }
        }
    }

    fn init_nic(&mut self, device_info: &DeviceInfo) -> Result<(), DriverError> {
        if self.initialized {
            return Err(DriverError::AlreadyInitialized);
        }
        
        // Decode BAR0 (MMIO base)
        let bar0 = device_info.bars[0];
        if bar0 == 0 {
            return Err(DriverError::DeviceNotFound);
        }
        
        let mmio_base = bar0 & !0xF;
        let mmio = MmioRegion::map(mmio_base, 0x20000).map_err(|_| DriverError::IoError)?;
        self.mmio = Some(mmio);
        self.irq = device_info.irq_line;
        
        // Read MAC
        self.read_mac();
        
        // Allocate rings
        let rx_desc_size = core::mem::size_of::<RxDesc>() * RX_DESC_COUNT;
        let tx_desc_size = core::mem::size_of::<TxDesc>() * TX_DESC_COUNT;
        
        let mut rx_ring = DmaBuffer::alloc(rx_desc_size, 4096).map_err(|_| DriverError::OutOfMemory)?;
        let mut tx_ring = DmaBuffer::alloc(tx_desc_size, 4096).map_err(|_| DriverError::OutOfMemory)?;
        
        // Allocate packet buffers (2KB per packet)
        let rx_buf_size = 2048 * RX_DESC_COUNT;
        let tx_buf_size = 2048 * TX_DESC_COUNT;
        
        let rx_bufs = DmaBuffer::alloc(rx_buf_size, 4096).map_err(|_| DriverError::OutOfMemory)?;
        let tx_bufs = DmaBuffer::alloc(tx_buf_size, 4096).map_err(|_| DriverError::OutOfMemory)?;
        
        let mmio = self.mmio.as_ref().unwrap();
        
        unsafe {
            // Initialize RX Descriptors
            let rx_descs = rx_ring.as_mut_slice_of::<RxDesc>(RX_DESC_COUNT);
            let rx_buf_phys = rx_bufs.phys_addr();
            
            for i in 0..RX_DESC_COUNT {
                rx_descs[i].addr = rx_buf_phys + (i * 2048) as u64;
                rx_descs[i].status = 0;
            }
            
            // Initialize TX Descriptors
            let tx_descs = tx_ring.as_mut_slice_of::<TxDesc>(TX_DESC_COUNT);
            for i in 0..TX_DESC_COUNT {
                tx_descs[i].addr = 0;
                tx_descs[i].cmd = 0;
                tx_descs[i].status = 1; // Done
            }
            
            // Program RCTL
            mmio.write_u32(E1000_RDBAL, (rx_ring.phys_addr() & 0xFFFFFFFF) as u32);
            mmio.write_u32(E1000_RDBAH, (rx_ring.phys_addr() >> 32) as u32);
            mmio.write_u32(E1000_RDLEN, rx_desc_size as u32);
            mmio.write_u32(E1000_RDH, 0);
            mmio.write_u32(E1000_RDT, (RX_DESC_COUNT - 1) as u32);
            
            mmio.write_u32(E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_SBP | E1000_RCTL_UPE | E1000_RCTL_MPE | E1000_RCTL_LPE | E1000_RCTL_BAM | E1000_RCTL_SECRC);
            
            // Program TCTL
            mmio.write_u32(E1000_TDBAL, (tx_ring.phys_addr() & 0xFFFFFFFF) as u32);
            mmio.write_u32(E1000_TDBAH, (tx_ring.phys_addr() >> 32) as u32);
            mmio.write_u32(E1000_TDLEN, tx_desc_size as u32);
            mmio.write_u32(E1000_TDH, 0);
            mmio.write_u32(E1000_TDT, 0);
            
            mmio.write_u32(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP);
            
            // Enable Interrupts
            mmio.write_u32(E1000_IMS, 0x1F6DC); // Enable all interrupts
        }
        
        self.rx_desc_ring = Some(rx_ring);
        self.tx_desc_ring = Some(tx_ring);
        self.rx_buffers = Some(rx_bufs);
        self.tx_buffers = Some(tx_bufs);
        
        self.initialized = true;
        Ok(())
    }
    
    fn send_packet(&mut self, data: &[u8]) -> Result<(), DriverError> {
        if !self.initialized { return Err(DriverError::NotInitialized); }
        
        let mmio = self.mmio.as_ref().unwrap();
        let tx_ring = self.tx_desc_ring.as_mut().unwrap();
        let tx_bufs = self.tx_buffers.as_ref().unwrap();
        
        unsafe {
            let tx_descs = tx_ring.as_mut_slice_of::<TxDesc>(TX_DESC_COUNT);
            let cur = self.tx_cur;
            
            // Copy data to buffer
            let buf_offset = cur * 2048;
            let buf_slice = tx_bufs.as_mut_slice();
            let len = data.len().min(2048);
            buf_slice[buf_offset..buf_offset+len].copy_from_slice(&data[0..len]);
            
            // Setup Descriptor
            tx_descs[cur].addr = tx_bufs.phys_addr() + buf_offset as u64;
            tx_descs[cur].length = len as u16;
            tx_descs[cur].cmd = E1000_CMD_EOP | E1000_CMD_IFCS | E1000_CMD_RS;
            tx_descs[cur].status = 0;
            
            // Advance Tail
            self.tx_cur = (cur + 1) % TX_DESC_COUNT;
            mmio.write_u32(E1000_TDT, self.tx_cur as u32);
        }
        
        Ok(())
    }
    
    fn receive_packet(&mut self, buffer: &mut [u8]) -> Result<usize, DriverError> {
        if !self.initialized { return Err(DriverError::NotInitialized); }
        
        let mmio = self.mmio.as_ref().unwrap();
        let rx_ring = self.rx_desc_ring.as_mut().unwrap();
        let rx_bufs = self.rx_buffers.as_ref().unwrap();
        
        unsafe {
            let rx_descs = rx_ring.as_mut_slice_of::<RxDesc>(RX_DESC_COUNT);
            let cur = self.rx_cur;
            
            if (rx_descs[cur].status & 1) != 0 { // DD bit set
                let len = rx_descs[cur].length as usize;
                let copy_len = len.min(buffer.len());
                
                let buf_offset = cur * 2048;
                let buf_slice = rx_bufs.as_slice();
                buffer[0..copy_len].copy_from_slice(&buf_slice[buf_offset..buf_offset+copy_len]);
                
                // Reset descriptor
                rx_descs[cur].status = 0;
                
                // Advance
                mmio.write_u32(E1000_RDT, cur as u32); // Inform hardware we processed this
                self.rx_cur = (cur + 1) % RX_DESC_COUNT;
                
                Ok(copy_len)
            } else {
                Err(DriverError::WouldBlock)
            }
        }
    }
    
    fn handle_ipc(&mut self) {
        let mut msg = IpcMessage::new();
        if ipc_receive(self.device_port, &mut msg).is_err() {
            return;
        }
        
        let mut response = IpcMessage::new();
        response.msg_type = driver_framework::ipc::IPC_MSG_RESPONSE;
        response.msg_id = msg.msg_id;
        
        // Handle network requests
        match msg.msg_id {
            NET_DEV_OP_SEND => {
                if let Ok(_) = self.send_packet(&msg.inline_data[0..msg.inline_size as usize]) {
                    response.inline_data[0] = 0;
                    response.inline_size = 1;
                } else {
                    response.inline_data[0] = 1;
                    response.inline_size = 1;
                }
            }
            NET_DEV_OP_RECEIVE => {
                let mut buf = [0u8; 1518];
                match self.receive_packet(&mut buf) {
                    Ok(len) => {
                        let copy_len = len.min(64);
                        response.inline_data[0..copy_len].copy_from_slice(&buf[0..copy_len]);
                        response.inline_size = copy_len as u32;
                    }
                    Err(_) => {
                        response.inline_data[0] = 1; // No packet
                        response.inline_size = 1;
                    }
                }
            }
            NET_DEV_OP_GET_MAC => {
                response.inline_data[0..6].copy_from_slice(&self.mac_address);
                response.inline_size = 6;
            }
            NET_DEV_OP_SET_IP => {
                response.inline_data[0] = 0;
                response.inline_size = 1;
            }
            _ => {}
        }
        
        let _ = ipc_send(self.device_port, &response);
    }
}

impl Driver for EthernetDriver {
    fn init(&mut self) -> Result<(), DriverError> {
        if self.initialized { return Err(DriverError::AlreadyInitialized); }
        self.device_port = ipc_create_port().map_err(|_| DriverError::IoError)?;
        
        // Register with device manager (placeholder)
        // driver_framework::register_device("ethernet", self.device_port);
        
        Ok(())
    }
    
    fn probe(&self, device_info: &DeviceInfo) -> bool {
        // E1000 check: Vendor 0x8086, Device 0x100E (or others)
        device_info.vendor_id == 0x8086 && (device_info.device_id == 0x100E || device_info.device_id == 0x100F)
    }
    
    fn start(&mut self) -> Result<(), DriverError> {
        // Initialized in init_nic
        Ok(())
    }
    
    fn stop(&mut self) -> Result<(), DriverError> {
        Ok(())
    }
    
    fn name(&self) -> &'static str { "ethernet_e1000" }
    fn version(&self) -> &'static str { "0.1.0" }
}

static mut DRIVER: EthernetDriver = EthernetDriver {
    initialized: false,
    device_port: 0,
    mmio: None,
    mac_address: [0; 6],
    irq: 0,
    rx_desc_ring: None,
    tx_desc_ring: None,
    rx_buffers: None,
    tx_buffers: None,
    rx_cur: 0,
    tx_cur: 0,
};

#[no_mangle]
pub extern "C" fn _start() -> ! {
    unsafe {
        // Find device (placeholder for scanning)
        let dev_info = DeviceInfo {
            vendor_id: 0x8086,
            device_id: 0x100E,
            class_code: 2,
            subclass: 0,
            prog_if: 0,
            revision: 0,
            bars: [0; 6], // This would need real values in a real run
            irq_line: 11,
            device_type: DeviceType::Pci,
        };
        
        if DRIVER.probe(&dev_info) {
            let _ = DRIVER.init();
            // DRIVER.init_nic(&dev_info); // Needs real BARs
        }
        
        loop {
            DRIVER.handle_ipc();
        }
    }
}
