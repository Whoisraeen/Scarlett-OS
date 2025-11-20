#![no_std]

//! USB Common Library
//!
//! Common USB structures, descriptors, and constants used across USB drivers.

/// USB Device Descriptor (18 bytes)
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
pub struct UsbDeviceDescriptor {
    pub length: u8,              // Size of this descriptor (18 bytes)
    pub descriptor_type: u8,     // DEVICE descriptor type (0x01)
    pub usb_version: u16,        // USB Specification Release Number (BCD)
    pub device_class: u8,        // Class code
    pub device_subclass: u8,     // Subclass code
    pub device_protocol: u8,     // Protocol code
    pub max_packet_size0: u8,    // Maximum packet size for endpoint 0
    pub vendor_id: u16,          // Vendor ID
    pub product_id: u16,         // Product ID
    pub device_version: u16,     // Device release number (BCD)
    pub manufacturer: u8,        // Index of manufacturer string descriptor
    pub product: u8,             // Index of product string descriptor
    pub serial_number: u8,       // Index of serial number string descriptor
    pub num_configurations: u8,  // Number of possible configurations
}

/// USB Configuration Descriptor
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
pub struct UsbConfigurationDescriptor {
    pub length: u8,
    pub descriptor_type: u8,
    pub total_length: u16,
    pub num_interfaces: u8,
    pub configuration_value: u8,
    pub configuration: u8,
    pub attributes: u8,
    pub max_power: u8,
}

/// USB Interface Descriptor
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
pub struct UsbInterfaceDescriptor {
    pub length: u8,
    pub descriptor_type: u8,
    pub interface_number: u8,
    pub alternate_setting: u8,
    pub num_endpoints: u8,
    pub interface_class: u8,
    pub interface_subclass: u8,
    pub interface_protocol: u8,
    pub interface: u8,
}

/// USB Endpoint Descriptor
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
pub struct UsbEndpointDescriptor {
    pub length: u8,
    pub descriptor_type: u8,
    pub endpoint_address: u8,
    pub attributes: u8,
    pub max_packet_size: u16,
    pub interval: u8,
}

/// USB Device Request (Setup Packet)
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
pub struct UsbDeviceRequest {
    pub request_type: u8,
    pub request: u8,
    pub value: u16,
    pub index: u16,
    pub length: u16,
}

/// USB Descriptor Types
pub const USB_DESC_TYPE_DEVICE: u8 = 0x01;
pub const USB_DESC_TYPE_CONFIGURATION: u8 = 0x02;
pub const USB_DESC_TYPE_STRING: u8 = 0x03;
pub const USB_DESC_TYPE_INTERFACE: u8 = 0x04;
pub const USB_DESC_TYPE_ENDPOINT: u8 = 0x05;

/// USB Device Classes
pub const USB_CLASS_HID: u8 = 0x03;
pub const USB_CLASS_HUB: u8 = 0x09;
pub const USB_CLASS_MASS_STORAGE: u8 = 0x08;

/// USB Standard Requests
pub const USB_REQ_GET_STATUS: u8 = 0x00;
pub const USB_REQ_CLEAR_FEATURE: u8 = 0x01;
pub const USB_REQ_SET_FEATURE: u8 = 0x03;
pub const USB_REQ_SET_ADDRESS: u8 = 0x05;
pub const USB_REQ_GET_DESCRIPTOR: u8 = 0x06;
pub const USB_REQ_SET_DESCRIPTOR: u8 = 0x07;
pub const USB_REQ_GET_CONFIGURATION: u8 = 0x08;
pub const USB_REQ_SET_CONFIGURATION: u8 = 0x09;

/// USB Request Types
pub const USB_REQ_TYPE_STANDARD: u8 = 0x00;
pub const USB_REQ_TYPE_CLASS: u8 = 0x20;
pub const USB_REQ_TYPE_VENDOR: u8 = 0x40;

/// USB Request Recipients
pub const USB_REQ_RECIPIENT_DEVICE: u8 = 0x00;
pub const USB_REQ_RECIPIENT_INTERFACE: u8 = 0x01;
pub const USB_REQ_RECIPIENT_ENDPOINT: u8 = 0x02;

/// USB Request Direction
pub const USB_REQ_DIRECTION_OUT: u8 = 0x00;
pub const USB_REQ_DIRECTION_IN: u8 = 0x80;

/// USB Endpoint Attributes
pub const USB_EP_TYPE_CONTROL: u8 = 0x00;
pub const USB_EP_TYPE_ISOCHRONOUS: u8 = 0x01;
pub const USB_EP_TYPE_BULK: u8 = 0x02;
pub const USB_EP_TYPE_INTERRUPT: u8 = 0x03;

/// USB Device Speed
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum UsbSpeed {
    Low = 1,      // 1.5 Mb/s
    Full = 2,     // 12 Mb/s
    High = 3,     // 480 Mb/s
    Super = 4,    // 5 Gb/s
    SuperPlus = 5, // 10 Gb/s
}

/// USB Device State
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum UsbDeviceState {
    Attached = 0,
    Powered = 1,
    Default = 2,
    Address = 3,
    Configured = 4,
    Suspended = 5,
}

/// USB Transfer Type
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UsbTransferType {
    Control,
    Bulk,
    Interrupt,
    Isochronous,
}

/// USB Transfer Direction
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UsbDirection {
    Out,
    In,
}
