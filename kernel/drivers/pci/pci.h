/**
 * @file pci.h
 * @brief PCI (Peripheral Component Interconnect) enumeration
 * 
 * Provides PCI bus enumeration and device detection.
 */

#ifndef KERNEL_DRIVERS_PCI_H
#define KERNEL_DRIVERS_PCI_H

#include "../../include/types.h"
#include "../../include/errors.h"

// PCI configuration space registers
#define PCI_CONFIG_VENDOR_ID    0x00
#define PCI_CONFIG_DEVICE_ID    0x02
#define PCI_CONFIG_COMMAND       0x04
#define PCI_CONFIG_STATUS        0x06
#define PCI_CONFIG_REVISION_ID   0x08
#define PCI_CONFIG_CLASS         0x0B
#define PCI_CONFIG_SUBCLASS      0x0A
#define PCI_CONFIG_PROG_IF       0x09
#define PCI_CONFIG_HEADER_TYPE  0x0E
#define PCI_CONFIG_BAR0          0x10
#define PCI_CONFIG_BAR1          0x14
#define PCI_CONFIG_BAR2          0x18
#define PCI_CONFIG_BAR3          0x1C
#define PCI_CONFIG_BAR4          0x20
#define PCI_CONFIG_BAR5          0x24

// PCI class codes
#define PCI_CLASS_MASS_STORAGE   0x01
#define PCI_SUBCLASS_SATA        0x06
#define PCI_PROG_IF_AHCI         0x01

// VirtIO device vendor ID
#define PCI_VENDOR_ID_VIRTIO     0x1AF4

// Network class
#define PCI_CLASS_NETWORK        0x02
#define PCI_SUBCLASS_ETHERNET    0x00

// BAR types
#define PCI_BAR_TYPE_MEMORY    0
#define PCI_BAR_TYPE_IO        1
#define PCI_BAR_TYPE_64BIT     4

// PCI device structure
typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t header_type;
    uint64_t bars[6];  // Base Address Registers
    uint8_t irq_line;  // IRQ line
    uint8_t irq_pin;   // IRQ pin
} pci_device_t;

// BAR information structure
typedef struct {
    uint64_t base_address;
    uint64_t size;
    bool is_io;
    bool is_64bit;
    bool is_prefetchable;
} pci_bar_info_t;

// Maximum PCI devices
#define MAX_PCI_DEVICES 256

// PCI functions
error_code_t pci_init(void);
error_code_t pci_enumerate(void);
pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id);
pci_device_t* pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if);
uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
uint32_t pci_get_device_count(void);
pci_device_t* pci_get_device(uint32_t index);

// BAR decoding
error_code_t pci_decode_bar(pci_device_t* dev, uint8_t bar_index, pci_bar_info_t* info);
uint64_t pci_get_bar_size(pci_device_t* dev, uint8_t bar_index);

// PCI Express detection
bool pci_is_pcie(pci_device_t* dev);

// IRQ management
uint8_t pci_get_irq_line(pci_device_t* dev);
uint8_t pci_get_irq_pin(pci_device_t* dev);

#endif // KERNEL_DRIVERS_PCI_H

