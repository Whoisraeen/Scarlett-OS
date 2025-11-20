//! XHCI Register Definitions
//!
//! Hardware register structures for XHCI controllers.

#[repr(C)]
pub struct XhciCapabilityRegs {
    pub caplength: u8,       // Capability register length
    pub _reserved: u8,
    pub hciversion: u16,     // Interface version number
    pub hcsparams1: u32,     // Structural parameters 1
    pub hcsparams2: u32,     // Structural parameters 2
    pub hcsparams3: u32,     // Structural parameters 3
    pub hccparams1: u32,     // Capability parameters 1
    pub dboff: u32,          // Doorbell offset
    pub rtsoff: u32,         // Runtime register space offset
    pub hccparams2: u32,     // Capability parameters 2
}

#[repr(C)]
pub struct XhciOperationalRegs {
    pub usbcmd: u32,         // USB Command
    pub usbsts: u32,         // USB Status
    pub pagesize: u32,       // Page size
    pub _reserved1: [u32; 2],
    pub dnctrl: u32,         // Device notification control
    pub crcr: u64,           // Command ring control
    pub _reserved2: [u32; 4],
    pub dcbaap: u64,         // Device context base address array pointer
    pub config: u32,         // Configure
}

#[repr(C)]
pub struct XhciRuntimeRegs {
    pub mfindex: u32,        // Microframe index
    pub _reserved: [u32; 7],
    pub interrupters: [XhciInterrupterRegs; 1024],
}

#[repr(C)]
pub struct XhciInterrupterRegs {
    pub iman: u32,           // Interrupter management
    pub imod: u32,           // Interrupter moderation
    pub erstsz: u32,         // Event ring segment table size
    pub _reserved: u32,
    pub erstba: u64,         // Event ring segment table base address
    pub erdp: u64,           // Event ring dequeue pointer
}

// USB Command Register (USBCMD) bits
pub const USBCMD_RUN_STOP: u32 = 1 << 0;   // Run/Stop
pub const USBCMD_HCRST: u32 = 1 << 1;      // Host controller reset
pub const USBCMD_INTE: u32 = 1 << 2;       // Interrupter enable
pub const USBCMD_HSEE: u32 = 1 << 3;       // Host system error enable
pub const USBCMD_EWE: u32 = 1 << 10;       // Enable wrap event

// USB Status Register (USBSTS) bits
pub const USBSTS_HCH: u32 = 1 << 0;        // HC halted
pub const USBSTS_HSE: u32 = 1 << 2;        // Host system error
pub const USBSTS_EINT: u32 = 1 << 3;       // Event interrupt
pub const USBSTS_PCD: u32 = 1 << 4;        // Port change detect
pub const USBSTS_SSS: u32 = 1 << 8;        // Save state status
pub const USBSTS_RSS: u32 = 1 << 9;        // Restore state status
pub const USBSTS_SRE: u32 = 1 << 10;       // Save/restore error
pub const USBSTS_CNR: u32 = 1 << 11;       // Controller not ready
pub const USBSTS_HCE: u32 = 1 << 12;       // Host controller error

// Command Ring Control Register (CRCR) bits
pub const CRCR_RCS: u64 = 1 << 0;          // Ring cycle state
pub const CRCR_CS: u64 = 1 << 1;           // Command stop
pub const CRCR_CA: u64 = 1 << 2;           // Command abort
pub const CRCR_CRR: u64 = 1 << 3;          // Command ring running

// Interrupter Management Register (IMAN) bits
pub const IMAN_IP: u32 = 1 << 0;           // Interrupt pending
pub const IMAN_IE: u32 = 1 << 1;           // Interrupt enable

// Port Status and Control Register (PORTSC) bits
pub const PORTSC_CCS: u32 = 1 << 0;        // Current connect status
pub const PORTSC_PED: u32 = 1 << 1;        // Port enabled/disabled
pub const PORTSC_OCA: u32 = 1 << 3;        // Over-current active
pub const PORTSC_PR: u32 = 1 << 4;         // Port reset
pub const PORTSC_PLS_MASK: u32 = 0xF << 5; // Port link state
pub const PORTSC_PP: u32 = 1 << 9;         // Port power
pub const PORTSC_SPEED_MASK: u32 = 0xF << 10; // Port speed
pub const PORTSC_CSC: u32 = 1 << 17;       // Connect status change
pub const PORTSC_PEC: u32 = 1 << 18;       // Port enabled/disabled change
pub const PORTSC_WRC: u32 = 1 << 19;       // Warm port reset change
pub const PORTSC_OCC: u32 = 1 << 20;       // Over-current change
pub const PORTSC_PRC: u32 = 1 << 21;       // Port reset change
