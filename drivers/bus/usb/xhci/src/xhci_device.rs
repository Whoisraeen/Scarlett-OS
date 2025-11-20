//! XHCI Device Context and Slot Management
//!
//! Device context structures for USB device communication.

/// Device Context
#[repr(C, align(64))]
pub struct DeviceContext {
    pub slot_context: SlotContext,
    pub endpoint_contexts: [EndpointContext; 31],
}

/// Slot Context
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct SlotContext {
    pub dw0: u32,  // Route string, speed, MTT, hub, context entries
    pub dw1: u32,  // Max exit latency, root hub port number, number of ports
    pub dw2: u32,  // TT hub slot ID, TT port number, TTT, interrupter target
    pub dw3: u32,  // Device address, slot state
    pub reserved: [u32; 4],
}

impl SlotContext {
    pub fn new() -> Self {
        Self {
            dw0: 0,
            dw1: 0,
            dw2: 0,
            dw3: 0,
            reserved: [0; 4],
        }
    }

    pub fn set_context_entries(&mut self, entries: u8) {
        self.dw0 = (self.dw0 & !0x1F0000) | ((entries as u32) << 27);
    }

    pub fn set_speed(&mut self, speed: UsbSpeed) {
        self.dw0 = (self.dw0 & !0xF00000) | ((speed as u32) << 20);
    }

    pub fn set_root_hub_port(&mut self, port: u8) {
        self.dw1 = (self.dw1 & !0xFF0000) | ((port as u32) << 16);
    }

    pub fn get_device_address(&self) -> u8 {
        (self.dw3 & 0xFF) as u8
    }

    pub fn get_slot_state(&self) -> SlotState {
        SlotState::from((self.dw3 >> 27) & 0x1F)
    }
}

/// Endpoint Context
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct EndpointContext {
    pub dw0: u32,  // EP state, mult, max primary streams, LSA, interval
    pub dw1: u32,  // EP type, HID, max burst size, max packet size
    pub dw2: u64,  // TR dequeue pointer
    pub dw4: u32,  // Average TRB length, max ESIT payload
    pub reserved: [u32; 3],
}

impl EndpointContext {
    pub fn new() -> Self {
        Self {
            dw0: 0,
            dw1: 0,
            dw2: 0,
            dw4: 0,
            reserved: [0; 3],
        }
    }

    pub fn set_ep_type(&mut self, ep_type: EndpointType) {
        self.dw1 = (self.dw1 & !0x38) | ((ep_type as u32) << 3);
    }

    pub fn set_max_packet_size(&mut self, size: u16) {
        self.dw1 = (self.dw1 & !0xFFFF0000) | ((size as u32) << 16);
    }

    pub fn set_dequeue_pointer(&mut self, ptr: u64) {
        self.dw2 = ptr | 1; // Set DCS bit
    }
}

/// USB Speed
#[repr(u8)]
#[derive(Debug, Clone, Copy)]
pub enum UsbSpeed {
    Full = 1,
    Low = 2,
    High = 3,
    Super = 4,
    SuperPlus = 5,
}

/// Slot State
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SlotState {
    DisabledOrEnabled = 0,
    Default = 1,
    Addressed = 2,
    Configured = 3,
    Unknown = 0xFF,
}

impl From<u32> for SlotState {
    fn from(value: u32) -> Self {
        match value {
            0 => SlotState::DisabledOrEnabled,
            1 => SlotState::Default,
            2 => SlotState::Addressed,
            3 => SlotState::Configured,
            _ => SlotState::Unknown,
        }
    }
}

/// Endpoint Type
#[repr(u8)]
#[derive(Debug, Clone, Copy)]
pub enum EndpointType {
    NotValid = 0,
    IsochOut = 1,
    BulkOut = 2,
    InterruptOut = 3,
    Control = 4,
    IsochIn = 5,
    BulkIn = 6,
    InterruptIn = 7,
}
