/**
 * @file ac97_driver.rs
 * @brief AC'97 Audio Codec Driver
 *
 * Legacy audio support for older hardware
 */

use core::ptr;

// AC'97 Register Offsets (NAM - Native Audio Mixer)
const AC97_RESET: u16 = 0x00;
const AC97_MASTER_VOLUME: u16 = 0x02;
const AC97_HEADPHONE_VOLUME: u16 = 0x04;
const AC97_MASTER_MONO_VOLUME: u16 = 0x06;
const AC97_PCM_OUT_VOLUME: u16 = 0x18;
const AC97_RECORD_SELECT: u16 = 0x1A;
const AC97_RECORD_GAIN: u16 = 0x1C;
const AC97_GENERAL_PURPOSE: u16 = 0x20;
const AC97_POWERDOWN: u16 = 0x26;
const AC97_EXTENDED_AUDIO_ID: u16 = 0x28;
const AC97_EXTENDED_AUDIO_STATUS: u16 = 0x2A;
const AC97_PCM_FRONT_DAC_RATE: u16 = 0x2C;
const AC97_PCM_LR_ADC_RATE: u16 = 0x32;

// NAM Bus Master Register Offsets
const AC97_NABM_BDBAR: u32 = 0x00;  // Buffer Descriptor Base Address
const AC97_NABM_CIV: u32 = 0x04;    // Current Index Value
const AC97_NABM_LVI: u32 = 0x05;    // Last Valid Index
const AC97_NABM_SR: u32 = 0x06;     // Status Register
const AC97_NABM_PICB: u32 = 0x08;   // Position in Current Buffer
const AC97_NABM_CR: u32 = 0x0B;     // Control Register

// Control Register Bits
const AC97_CR_RPBM: u8 = 1 << 0;    // Run/Pause Bus Master
const AC97_CR_RR: u8 = 1 << 1;      // Reset Registers
const AC97_CR_LVBIE: u8 = 1 << 2;   // Last Valid Buffer Interrupt Enable
const AC97_CR_FEIE: u8 = 1 << 3;    // FIFO Error Interrupt Enable
const AC97_CR_IOCE: u8 = 1 << 4;    // Interrupt On Completion Enable

// Status Register Bits
const AC97_SR_DCH: u8 = 1 << 0;     // DMA Controller Halted
const AC97_SR_CELV: u8 = 1 << 1;    // Current Equals Last Valid
const AC97_SR_LVBCI: u8 = 1 << 2;   // Last Valid Buffer Completion Interrupt
const AC97_SR_BCIS: u8 = 1 << 3;    // Buffer Completion Interrupt Status
const AC97_SR_FIFOE: u8 = 1 << 4;   // FIFO Error

// Buffer Descriptor Entry
#[repr(C, packed)]
struct Ac97BdEntry {
    buffer_addr: u32,
    control: u32,  // [31:16] = length in samples, [15] = BUP, [14] = IOC
}

// AC'97 Channel (PCM Out, PCM In, Mic In)
pub struct Ac97Channel {
    base_addr: usize,
    bd_list: Vec<Ac97BdEntry>,
    buffer: Vec<u8>,
    running: bool,
}

/**
 * @file ac97_driver.rs
 * @brief AC'97 Audio Codec Driver
 *
 * User-space AC'97 driver using driver_framework
 */

#![no_std]

use driver_framework::{Driver, DeviceInfo, DeviceType, DriverError};
use driver_framework::io::IoPort;
use driver_framework::dma::DmaBuffer;
use driver_framework::ipc::ipc_create_port;

// AC'97 Register Offsets
const AC97_RESET: u16 = 0x00;
const AC97_MASTER_VOLUME: u16 = 0x02;
const AC97_PCM_OUT_VOLUME: u16 = 0x18;
const AC97_EXTENDED_AUDIO_ID: u16 = 0x28;
const AC97_EXTENDED_AUDIO_STATUS: u16 = 0x2A;
const AC97_PCM_FRONT_DAC_RATE: u16 = 0x2C;

// NAM Bus Master Register Offsets
const AC97_NABM_BDBAR: u16 = 0x00;
const AC97_NABM_CIV: u16 = 0x04;
const AC97_NABM_LVI: u16 = 0x05;
const AC97_NABM_SR: u16 = 0x06;
const AC97_NABM_CR: u16 = 0x0B;

// Control Register Bits
const AC97_CR_RPBM: u8 = 1 << 0;
const AC97_CR_RR: u8 = 1 << 1;
const AC97_CR_IOCE: u8 = 1 << 4;

// Buffer Descriptor Entry
#[repr(C, packed)]
struct Ac97BdEntry {
    buffer_addr: u32,
    control: u32,
}

pub struct Ac97Driver {
    initialized: bool,
    nam_bar: Option<u16>,
    nabm_bar: Option<u16>,
    irq: u8,
    bd_list: Option<DmaBuffer>,
    pcm_buffer: Option<DmaBuffer>,
}

impl Ac97Driver {
    pub fn new() -> Self {
        Self {
            initialized: false,
            nam_bar: None,
            nabm_bar: None,
            irq: 0,
            bd_list: None,
            pcm_buffer: None,
        }
    }

    fn read_nam(&self, reg: u16) -> u16 {
        if let Some(base) = self.nam_bar {
            unsafe { IoPort::new(base + reg).read_u16() }
        } else { 0 }
    }

    fn write_nam(&self, reg: u16, val: u16) {
        if let Some(base) = self.nam_bar {
            unsafe { IoPort::new(base + reg).write_u16(val) }
        }
    }

    fn read_nabm_u8(&self, offset: u16) -> u8 {
        if let Some(base) = self.nabm_bar {
            unsafe { IoPort::new(base + offset).read_u8() }
        } else { 0 }
    }

    fn write_nabm_u8(&self, offset: u16, val: u8) {
        if let Some(base) = self.nabm_bar {
            unsafe { IoPort::new(base + offset).write_u8(val) }
        }
    }

    fn write_nabm_u32(&self, offset: u16, val: u32) {
        if let Some(base) = self.nabm_bar {
            unsafe { IoPort::new(base + offset).write_u32(val) }
        }
    }
}

impl Driver for Ac97Driver {
    fn name(&self) -> &'static str { "ac97" }
    
    fn probe(&self, dev: &DeviceInfo) -> bool {
        // AC'97 Multimedia Audio Controller (Class 0x04, Subclass 0x01)
        dev.class_code == 0x04 && dev.subclass == 0x01
    }

    fn init(&mut self) -> Result<(), DriverError> {
        if self.initialized { return Err(DriverError::AlreadyInitialized); }
        
        // Register with framework handled by caller/framework
        Ok(())
    }

    fn start(&mut self) -> Result<(), DriverError> {
        // Initialize hardware
        // Reset
        self.write_nam(AC97_RESET, 1); // Any write resets
        
        // Set Volume
        self.write_nam(AC97_MASTER_VOLUME, 0x0000); // Unmute, max vol
        self.write_nam(AC97_PCM_OUT_VOLUME, 0x0808); // ~0dB
        
        // Allocate BD List
        let bd_size = 32 * core::mem::size_of::<Ac97BdEntry>();
        let mut bd = DmaBuffer::alloc(bd_size, 4096).map_err(|_| DriverError::OutOfMemory)?;
        
        // Allocate PCM buffer
        let pcm = DmaBuffer::alloc(65536, 4096).map_err(|_| DriverError::OutOfMemory)?;
        
        // Setup BD Entry
        unsafe {
            let entries = bd.as_mut_slice_of::<Ac97BdEntry>(32);
            entries[0].buffer_addr = pcm.phys_addr() as u32;
            entries[0].control = (16384 << 16) | 0x8000; // Length (samples) | IOC
        }
        
        self.bd_list = Some(bd);
        self.pcm_buffer = Some(pcm);
        
        // Setup Bus Master
        // Reset PCM Out
        self.write_nabm_u8(AC97_NABM_CR + 0x10, AC97_CR_RR);
        // Wait?
        
        // Set BDBAR
        if let Some(ref bd) = self.bd_list {
             self.write_nabm_u32(AC97_NABM_BDBAR + 0x10, bd.phys_addr() as u32);
        }
        
        // Set LVI
        self.write_nabm_u8(AC97_NABM_LVI + 0x10, 0);
        
        // Enable interrupts and Bus Master
        // self.write_nabm_u8(AC97_NABM_CR + 0x10, AC97_CR_IOCE | AC97_CR_RPBM);
        
        self.initialized = true;
        Ok(())
    }

    fn stop(&mut self) -> Result<(), DriverError> {
        // Stop Bus Master
        self.write_nabm_u8(AC97_NABM_CR + 0x10, 0);
        self.initialized = false;
        Ok(())
    }
}

// Initialization helper called by framework
pub fn init_driver(dev: &DeviceInfo, driver: &mut Ac97Driver) -> Result<(), DriverError> {
    // Get BARs
    let nam = dev.bars[0] & !1; // IO Space
    let nabm = dev.bars[1] & !1;
    
    if nam == 0 || nabm == 0 { return Err(DriverError::DeviceNotFound); }
    
    driver.nam_bar = Some(nam as u16);
    driver.nabm_bar = Some(nabm as u16);
    driver.irq = dev.irq_line;
    
    driver.init()
}

