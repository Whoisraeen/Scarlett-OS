/**
 * @file hda_driver.rs
 * @brief Intel High Definition Audio (HDA) Driver
 *
 * User-space HDA driver for Scarlett OS
 */

#![no_std]
#![allow(unused_imports)] // Allow unused imports for now

use core::ptr;
extern crate alloc;
use alloc::vec::Vec;
use driver_framework::{Driver, DriverError, DeviceInfo, DeviceType};
use driver_framework::mmio::MmioRegion;
use driver_framework::dma::DmaBuffer;
use driver_framework::syscalls::{sys_sleep, sys_get_uptime_ms};
use driver_framework::ipc::ipc_create_port;

// HDA PCI IDs
const HDA_VENDOR_INTEL: u16 = 0x8086;
const HDA_VENDOR_AMD: u16 = 0x1002;
const HDA_VENDOR_NVIDIA: u16 = 0x10DE;

// HDA Register Offsets
const HDA_REG_GCAP: u32 = 0x00;      // Global Capabilities
const HDA_REG_VMIN: u32 = 0x02;      // Minor Version
const HDA_REG_VMAJ: u32 = 0x03;      // Major Version
const HDA_REG_OUTPAY: u32 = 0x04;    // Output Payload Capability
const HDA_REG_INPAY: u32 = 0x06;     // Input Payload Capability
const HDA_REG_GCTL: u32 = 0x08;      // Global Control
const HDA_REG_WAKEEN: u32 = 0x0C;    // Wake Enable
const HDA_REG_STATESTS: u32 = 0x0E;  // State Change Status
const HDA_REG_GSTS: u32 = 0x10;      // Global Status
const HDA_REG_OUTSTRMPAY: u32 = 0x18;// Output Stream Payload Capability
const HDA_REG_INSTRMPAY: u32 = 0x1A; // Input Stream Payload Capability
const HDA_REG_INTCTL: u32 = 0x20;    // Interrupt Control
const HDA_REG_INTSTS: u32 = 0x24;    // Interrupt Status
const HDA_REG_WALCLK: u32 = 0x30;    // Wall Clock Counter
const HDA_REG_SSYNC: u32 = 0x38;     // Stream Synchronization

// Stream Descriptor Registers (per stream)
const HDA_SD_CTL: u32 = 0x00;        // Stream Control
const HDA_SD_STS: u32 = 0x03;        // Stream Status
const HDA_SD_LPIB: u32 = 0x04;       // Link Position in Buffer
const HDA_SD_CBL: u32 = 0x08;        // Cyclic Buffer Length
const HDA_SD_LVI: u32 = 0x0C;        // Last Valid Index
const HDA_SD_FIFOW: u32 = 0x0E;      // FIFO Watermark
const HDA_SD_FIFOS: u32 = 0x10;      // FIFO Size
const HDA_SD_FMT: u32 = 0x12;        // Stream Format
const HDA_SD_BDPL: u32 = 0x18;       // Buffer Descriptor List Pointer Lower
const HDA_SD_BDPU: u32 = 0x1C;       // Buffer Descriptor List Pointer Upper

// Control Register Bits
const HDA_GCTL_CRST: u32 = 1 << 0;   // Controller Reset
const HDA_GCTL_FCNTRL: u32 = 1 << 1; // Flush Control
const HDA_GCTL_UNSOL: u32 = 1 << 8;  // Accept Unsolicited Response Enable

// Stream Control Bits
const HDA_SD_CTL_SRST: u32 = 1 << 0; // Stream Reset
const HDA_SD_CTL_RUN: u32 = 1 << 1;  // Stream Run
const HDA_SD_CTL_IOCE: u32 = 1 << 2; // Interrupt On Completion Enable
const HDA_SD_CTL_FEIE: u32 = 1 << 3; // FIFO Error Interrupt Enable
const HDA_SD_CTL_DEIE: u32 = 1 << 4; // Descriptor Error Interrupt Enable

// Buffer Descriptor List Entry
#[repr(C, packed)]
#[derive(Clone, Copy)]
struct HdaBdlEntry {
    address: u64,      // Buffer address
    length: u32,       // Buffer length
    ioc: u32,          // Interrupt on completion
}

// HDA Stream
#[derive(Clone)]
pub struct HdaStream {
    id: u8,
    base_addr: usize,
    bdl_buffer: Option<DmaBuffer>, // DMA buffer for BDL
    bdl_entries: Vec<HdaBdlEntry>, // Entries in the BDL
    data_buffer: Option<DmaBuffer>, // DMA buffer for audio data
    running: bool,
}

// HDA Codec
#[derive(Clone)]
pub struct HdaCodec {
    address: u8,
    vendor_id: u32,
    device_id: u32,
    revision_id: u32,
}

// HDA Controller
#[derive(Clone)]
pub struct HdaController {
    pci_device_info: DeviceInfo,
    mmio: Option<MmioRegion>,
    irq: u8,
    
    // Capabilities
    oss: u8,           // Number of Output Streams Supported
    iss: u8,           // Number of Input Streams Supported
    bss: u8,           // Number of Bidirectional Streams Supported
    
    // Codecs
    codecs: Vec<HdaCodec>,
    
    // Streams
    output_streams: Vec<HdaStream>,
    input_streams: Vec<HdaStream>,
}

impl HdaController {
    /// Create new HDA controller
    pub fn new(pci_device: DeviceInfo) -> Result<Self, DriverError> {
        let bar0 = pci_device.bars[0];
        if bar0 == 0 {
            return Err(DriverError::DeviceNotFound);
        }
        
        // Map MMIO region from PCI BAR0
        let mmio_base = bar0 & !0xF;
        let mmio_size = 0x4000; // Typical HDA MMIO size
        let mmio = MmioRegion::map(mmio_base, mmio_size).map_err(|_| DriverError::IoError)?;
        
        Ok(HdaController {
            pci_device_info: pci_device,
            mmio: Some(mmio),
            irq: pci_device.irq_line,
            oss: 0,
            iss: 0,
            bss: 0,
            codecs: Vec::new(),
            output_streams: Vec::new(),
            input_streams: Vec::new(),
        })
    }
    
    /// Initialize HDA controller
    pub fn init(&mut self) -> Result<(), &'static str> {
        // Reset controller
        self.reset()?;
        
        // Read capabilities
        self.read_capabilities();
        
        // Enumerate codecs
        self.enumerate_codecs()?;
        
        // Initialize streams
        self.init_streams()?;
        
        Ok(())
    }
    
    /// Reset HDA controller
    fn reset(&mut self) -> Result<(), &'static str> {
        let mmio = self.mmio.as_ref().ok_or("MMIO not mapped")?;

        // Clear CRST bit
        mmio.write_u32(HDA_REG_GCTL as usize, 0);
        
        // Wait for reset
        for _ in 0..1000 {
            if mmio.read_u32(HDA_REG_GCTL as usize) & HDA_GCTL_CRST == 0 {
                break;
            }
            sys_sleep(1); // Sleep for 1ms
        }
        
        // Set CRST bit
        mmio.write_u32(HDA_REG_GCTL as usize, HDA_GCTL_CRST);
        
        // Wait for controller ready
        for _ in 0..1000 {
            if mmio.read_u32(HDA_REG_GCTL as usize) & HDA_GCTL_CRST != 0 {
                return Ok(());
            }
            sys_sleep(1); // Sleep for 1ms
        }
        
        Err("HDA controller reset timeout")
    }
    
    /// Read capabilities
    fn read_capabilities(&mut self) {
        let mmio = self.mmio.as_ref().unwrap();
        let gcap = mmio.read_u16(HDA_REG_GCAP as usize);
        self.oss = ((gcap >> 12) & 0x0F) as u8;
        self.iss = ((gcap >> 8) & 0x0F) as u8;
        self.bss = ((gcap >> 3) & 0x1F) as u8;
    }
    
    /// Enumerate codecs
    fn enumerate_codecs(&mut self) -> Result<(), &'static str> {
        let mmio = self.mmio.as_ref().unwrap();
        let statests = mmio.read_u16(HDA_REG_STATESTS as usize);
        
        for i in 0..15 {
            if statests & (1 << i) != 0 {
                // Codec present at address i
                let codec = HdaCodec {
                    address: i as u8,
                    vendor_id: 0, // Read from codec registers
                    device_id: 0,
                    revision_id: 0,
                };
                self.codecs.push(codec);
            }
        }
        
        Ok(())
    }
    
    /// Initialize streams
    fn init_streams(&mut self) -> Result<(), &'static str> {
        // Initialize output streams
        for i in 0..self.oss {
            let stream = HdaStream {
                id: i,
                base_addr: self.mmio.as_ref().unwrap().base_virt_addr() + 0x80 + (i as usize * 0x20),
                bdl_buffer: None,
                bdl_entries: Vec::new(),
                data_buffer: None,
                running: false,
            };
            self.output_streams.push(stream);
        }
        
        // Initialize input streams
        for i in 0..self.iss {
            let stream = HdaStream {
                id: i,
                base_addr: self.mmio.as_ref().unwrap().base_virt_addr() + 0x80 + ((self.oss + i) as usize * 0x20),
                bdl_buffer: None,
                bdl_entries: Vec::new(),
                data_buffer: None,
                running: false,
            };
            self.input_streams.push(stream);
        }
        
        Ok(())
    }
    
    /// Start playback stream
    pub fn start_playback(&mut self, stream_id: u8, buffer: DmaBuffer, sample_rate: u32, channels: u8) -> Result<(), &'static str> {
        if stream_id >= self.oss {
            return Err("Invalid stream ID");
        }
        
        let stream = &mut self.output_streams[stream_id as usize];
        
        // Setup buffer descriptor list
        self.setup_bdl(stream, buffer)?;
        
        // Set stream format
        let format = self.encode_format(sample_rate, channels, 16);
        self.write_stream_reg16(stream, HDA_SD_FMT, format);
        
        // Set cyclic buffer length
        self.write_stream_reg32(stream, HDA_SD_CBL, stream.data_buffer.as_ref().unwrap().size() as u32);
        
        // Set last valid index
        self.write_stream_reg16(stream, HDA_SD_LVI, (stream.bdl_entries.len() - 1) as u16);
        
        // Enable interrupts and start stream
        let ctl = HDA_SD_CTL_RUN | HDA_SD_CTL_IOCE | HDA_SD_CTL_FEIE | HDA_SD_CTL_DEIE;
        self.write_stream_reg32(stream, HDA_SD_CTL, ctl);
        
        stream.running = true;
        
        Ok(())
    }
    
    /// Stop playback stream
    pub fn stop_playback(&mut self, stream_id: u8) -> Result<(), &'static str> {
        if stream_id >= self.oss {
            return Err("Invalid stream ID");
        }
        
        let stream = &mut self.output_streams[stream_id as usize];
        
        // Stop stream
        self.write_stream_reg32(stream, HDA_SD_CTL, 0);
        
        stream.running = false;
        
        Ok(())
    }
    
    /// Setup buffer descriptor list
    fn setup_bdl(&self, stream: &mut HdaStream, data_buffer: DmaBuffer) -> Result<(), &'static str> {
        // HDA BDLs require 128-byte alignment
        let bdl_buffer = DmaBuffer::alloc(core::mem::size_of::<HdaBdlEntry>() * 2, 128).map_err(|_| "Failed to allocate BDL buffer")?;
        
        let entry = HdaBdlEntry {
            address: data_buffer.phys_addr(),
            length: data_buffer.size() as u32,
            ioc: 1,  // Interrupt on completion
        };
        
        unsafe {
            let bdl_ptr = bdl_buffer.as_mut_ptr() as *mut HdaBdlEntry;
            ptr::write_volatile(bdl_ptr, entry);
        }
        
        stream.bdl_buffer = Some(bdl_buffer.clone());
        stream.data_buffer = Some(data_buffer);
        stream.bdl_entries.push(entry);
        
        // Write BDL pointer to stream descriptor
        let bdl_phys_addr = bdl_buffer.phys_addr();
        self.write_stream_reg32(stream, HDA_SD_BDPL, (bdl_phys_addr & 0xFFFFFFFF) as u32);
        self.write_stream_reg32(stream, HDA_SD_BDPU, (bdl_phys_addr >> 32) as u32);
        
        Ok(())
    }
    
    /// Encode stream format
    fn encode_format(&self, sample_rate: u32, channels: u8, bits: u8) -> u16 {
        let mut format: u16 = 0;
        
        // Sample rate
        format |= match sample_rate {
            8000 => 0x0,
            11025 => 0x1,
            16000 => 0x2,
            22050 => 0x3,
            32000 => 0x4,
            44100 => 0x5,
            48000 => 0x6,
            88200 => 0x7,
            96000 => 0x8,
            192000 => 0x9,
            _ => 0x6,  // Default to 48kHz
        };
        
        // Channels (0 = 1 channel, 1 = 2 channels, etc.)
        format |= ((channels - 1) as u16) << 4;
        
        // Bits per sample
        format |= match bits {
            8 => 0x0 << 8,
            16 => 0x1 << 8,
            20 => 0x2 << 8,
            24 => 0x3 << 8,
            32 => 0x4 << 8,
            _ => 0x1 << 8,  // Default to 16-bit
        };
        
        format
    }
    
    /// Read 32-bit register
    fn read_reg32(&self, offset: u32) -> u32 {
        let mmio = self.mmio.as_ref().unwrap();
        mmio.read_u32(offset as usize)
    }
    
    /// Write 32-bit register
    fn write_reg32(&self, offset: u32, value: u32) {
        let mmio = self.mmio.as_ref().unwrap();
        mmio.write_u32(offset as usize, value)
    }
    
    /// Read 16-bit register
    fn read_reg16(&self, offset: u32) -> u16 {
        let mmio = self.mmio.as_ref().unwrap();
        mmio.read_u16(offset as usize)
    }
    
    /// Write 16-bit register
    fn write_reg16(&self, offset: u32, value: u16) {
        let mmio = self.mmio.as_ref().unwrap();
        mmio.write_u16(offset as usize, value)
    }
    
    /// Read stream register (32-bit)
    fn read_stream_reg32(&self, stream: &HdaStream, offset: u32) -> u32 {
        let mmio = self.mmio.as_ref().unwrap();
        mmio.read_u32((stream.base_addr - mmio.base_virt_addr()) + offset as usize)
    }
    
    /// Write stream register (32-bit)
    fn write_stream_reg32(&self, stream: &HdaStream, offset: u32, value: u32) {
        let mmio = self.mmio.as_ref().unwrap();
        mmio.write_u32((stream.base_addr - mmio.base_virt_addr()) + offset as usize, value)
    }
    
    /// Write stream register (16-bit)
    fn write_stream_reg16(&self, stream: &HdaStream, offset: u32, value: u16) {
        let mmio = self.mmio.as_ref().unwrap();
        mmio.write_u16((stream.base_addr - mmio.base_virt_addr()) + offset as usize, value)
    }
}

// Driver entry point
pub struct HdaDriver {
    initialized: bool,
    device_port: u64,
    controllers: Vec<HdaController>,
}

impl HdaDriver {
    pub fn new() -> Self {
        Self {
            initialized: false,
            device_port: 0,
            controllers: Vec::new(),
        }
    }
}

impl Driver for HdaDriver {
    fn name(&self) -> &'static str { "hda" }
    fn probe(&self, device_info: &DeviceInfo) -> bool {
        // HDA controller class code is 0x0403 (Audio Device)
        device_info.device_type == DeviceType::Pci &&
        device_info.class_code == 0x04 &&
        device_info.subclass == 0x03
    }
    fn init(&mut self) -> Result<(), DriverError> {
        if self.initialized { return Err(DriverError::AlreadyInitialized); }
        self
            .device_port = ipc_create_port().map_err(|_| DriverError::IoError)?;

        // Enumerate PCI devices and find HDA controllers
        // This should be handled by the framework calling probe and then start
        // For standalone init, we would enumerate here.
        // Assuming probe will provide the device_info needed for HdaController::new
        
        self.initialized = true;
        Ok(())
    }
    fn start(&mut self) -> Result<(), DriverError> {
        // A controller is created for each probed device.
        // This is called by the framework after probe returns true.
        // Assuming device_info is passed here from the framework.
        let device_info = self.pci_device_info; // This needs to be stored or passed by framework
        let mut controller = HdaController::new(device_info)?;
        controller.init().map_err(|_| DriverError::InitFailed)?;
        self.controllers.push(controller);
        Ok(())
    }
    fn stop(&mut self) -> Result<(), DriverError> {
        self.controllers.clear();
        Ok(())
    }
}

#[no_mangle]
pub extern "C" fn _start() -> ! {
    // This is the main entry point for the HDA driver service.
    // It should register itself with the driver framework.
    // For now, it simply loops, waiting for the framework to interact.
    // The actual driver framework entry point will handle the init/probe/start lifecycle.
    loop {
        sys_sleep(100); // Sleep to avoid busy-looping
    }
}