//! XHCI Transfer Request Block (TRB) Definitions
//!
//! TRBs are the basic data structures for communication with the XHCI controller.

/// Transfer Request Block (TRB)
#[repr(C, align(16))]
#[derive(Debug, Clone, Copy)]
pub struct Trb {
    pub parameter: u64,
    pub status: u32,
    pub control: u32,
}

impl Trb {
    pub fn new() -> Self {
        Self {
            parameter: 0,
            status: 0,
            control: 0,
        }
    }

    pub fn set_type(&mut self, trb_type: TrbType) {
        self.control = (self.control & !0xFC00) | ((trb_type as u32) << 10);
    }

    pub fn get_type(&self) -> TrbType {
        TrbType::from((self.control >> 10) & 0x3F)
    }

    pub fn set_cycle_bit(&mut self, cycle: bool) {
        if cycle {
            self.control |= 1;
        } else {
            self.control &= !1;
        }
    }

    pub fn get_cycle_bit(&self) -> bool {
        (self.control & 1) != 0
    }
}

/// TRB Types
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TrbType {
    // Transfer TRBs
    Normal = 1,
    SetupStage = 2,
    DataStage = 3,
    StatusStage = 4,
    Isoch = 5,
    Link = 6,
    EventData = 7,
    NoOp = 8,

    // Command TRBs
    EnableSlot = 9,
    DisableSlot = 10,
    AddressDevice = 11,
    ConfigureEndpoint = 12,
    EvaluateContext = 13,
    ResetEndpoint = 14,
    StopEndpoint = 15,
    SetTRDequeuePointer = 16,
    ResetDevice = 17,
    ForceEvent = 18,
    NegotiateBandwidth = 19,
    SetLatencyToleranceValue = 20,
    GetPortBandwidth = 21,
    ForceHeader = 22,
    NoOpCommand = 23,

    // Event TRBs
    TransferEvent = 32,
    CommandCompletionEvent = 33,
    PortStatusChangeEvent = 34,
    BandwidthRequestEvent = 35,
    DoorbellEvent = 36,
    HostControllerEvent = 37,
    DeviceNotificationEvent = 38,
    MFINDEXWrapEvent = 39,

    Unknown = 0xFF,
}

impl From<u32> for TrbType {
    fn from(value: u32) -> Self {
        match value {
            1 => TrbType::Normal,
            2 => TrbType::SetupStage,
            3 => TrbType::DataStage,
            4 => TrbType::StatusStage,
            5 => TrbType::Isoch,
            6 => TrbType::Link,
            7 => TrbType::EventData,
            8 => TrbType::NoOp,
            9 => TrbType::EnableSlot,
            10 => TrbType::DisableSlot,
            11 => TrbType::AddressDevice,
            12 => TrbType::ConfigureEndpoint,
            13 => TrbType::EvaluateContext,
            14 => TrbType::ResetEndpoint,
            15 => TrbType::StopEndpoint,
            16 => TrbType::SetTRDequeuePointer,
            17 => TrbType::ResetDevice,
            18 => TrbType::ForceEvent,
            19 => TrbType::NegotiateBandwidth,
            20 => TrbType::SetLatencyToleranceValue,
            21 => TrbType::GetPortBandwidth,
            22 => TrbType::ForceHeader,
            23 => TrbType::NoOpCommand,
            32 => TrbType::TransferEvent,
            33 => TrbType::CommandCompletionEvent,
            34 => TrbType::PortStatusChangeEvent,
            35 => TrbType::BandwidthRequestEvent,
            36 => TrbType::DoorbellEvent,
            37 => TrbType::HostControllerEvent,
            38 => TrbType::DeviceNotificationEvent,
            39 => TrbType::MFINDEXWrapEvent,
            _ => TrbType::Unknown,
        }
    }
}

/// TRB Completion Codes
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TrbCompletionCode {
    Success = 1,
    DataBufferError = 2,
    BabbleDetectedError = 3,
    USBTransactionError = 4,
    TRBError = 5,
    StallError = 6,
    ResourceError = 7,
    BandwidthError = 8,
    NoSlotsAvailableError = 9,
    InvalidStreamTypeError = 10,
    SlotNotEnabledError = 11,
    EndpointNotEnabledError = 12,
    ShortPacket = 13,
    RingUnderrun = 14,
    RingOverrun = 15,
    VFEventRingFullError = 16,
    ParameterError = 17,
    BandwidthOverrunError = 18,
    ContextStateError = 19,
    NoPingResponseError = 20,
    EventRingFullError = 21,
    IncompatibleDeviceError = 22,
    MissedServiceError = 23,
    CommandRingStopped = 24,
    CommandAborted = 25,
    Stopped = 26,
    StoppedLengthInvalid = 27,
    MaxExitLatencyTooLargeError = 29,
    IsochBufferOverrun = 31,
    EventLostError = 32,
    UndefinedError = 33,
    InvalidStreamIDError = 34,
    SecondaryBandwidthError = 35,
    SplitTransactionError = 36,
}
