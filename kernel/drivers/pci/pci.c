/**
 * @file pci.c
 * @brief PCI enumeration implementation
 */

#include "../../include/types.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "../../include/mm/heap.h"
#include "../../include/string.h"
#include "pci.h"

// PCI devices
static pci_device_t pci_devices[MAX_PCI_DEVICES];
static uint32_t pci_device_count = 0;

// PCI configuration space I/O ports
#define PCI_CONFIG_ADDRESS   0xCF8
#define PCI_CONFIG_DATA      0xCFC

/**
 * Read PCI configuration register (32-bit)
 */
static uint32_t pci_read_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    // Build configuration address
    uint32_t address = (1 << 31) |                    // Enable bit
                       ((uint32_t)bus << 16) |        // Bus number
                       ((uint32_t)device << 11) |     // Device number
                       ((uint32_t)function << 8) |     // Function number
                       (offset & 0xFC);                // Register offset (aligned to 4 bytes)
    
    // Write address
    __asm__ volatile("outl %0, %1" : : "a"(address), "Nd"(PCI_CONFIG_ADDRESS));
    
    // Read data
    uint32_t data;
    __asm__ volatile("inl %1, %0" : "=a"(data) : "Nd"(PCI_CONFIG_DATA));
    
    return data;
}

/**
 * Write PCI configuration register (32-bit)
 */
static void pci_write_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    // Build configuration address
    uint32_t address = (1 << 31) |                    // Enable bit
                       ((uint32_t)bus << 16) |        // Bus number
                       ((uint32_t)device << 11) |     // Device number
                       ((uint32_t)function << 8) |     // Function number
                       (offset & 0xFC);                // Register offset (aligned to 4 bytes)
    
    // Write address
    __asm__ volatile("outl %0, %1" : : "a"(address), "Nd"(PCI_CONFIG_ADDRESS));
    
    // Write data
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(PCI_CONFIG_DATA));
}

/**
 * Read PCI configuration register (16-bit or 8-bit)
 */
uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t data = pci_read_config_dword(bus, device, function, offset & 0xFC);
    
    // Extract appropriate bytes
    uint8_t byte_offset = offset & 0x3;
    if (byte_offset == 0) {
        return data & 0xFFFF;  // Lower 16 bits
    } else if (byte_offset == 2) {
        return (data >> 16) & 0xFFFF;  // Upper 16 bits
    } else {
        return (data >> (byte_offset * 8)) & 0xFF;  // Single byte
    }
}

/**
 * Write PCI configuration register
 */
void pci_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    // For simplicity, we'll only support 32-bit writes at 4-byte aligned offsets
    if ((offset & 0x3) == 0) {
        pci_write_config_dword(bus, device, function, offset, value);
    }
}

/**
 * Check if PCI device exists
 */
static bool pci_device_exists(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t vendor_id = pci_read_config(bus, device, function, PCI_CONFIG_VENDOR_ID);
    return (vendor_id != 0xFFFF);
}

/**
 * Enumerate PCI devices
 */
error_code_t pci_enumerate(void) {
    kinfo("Enumerating PCI devices...\n");
    
    pci_device_count = 0;
    memset(pci_devices, 0, sizeof(pci_devices));
    
    // Scan all buses, devices, and functions
    for (uint16_t bus = 0; bus < 256; bus++) {
        // Skip bus if device 0:0 doesn't exist (optimization for single-bus systems)
        if (!pci_device_exists(bus, 0, 0)) {
            // If this is not bus 0 and device 0:0 doesn't exist, skip remaining buses
            if (bus > 0) {
                break;  // No more buses
            }
            continue;  // Bus 0 might have devices at other slots
        }

        for (uint8_t device = 0; device < 32; device++) {
            uint8_t functions = 1;  // Start with 1 function

            // Check if device exists
            if (!pci_device_exists(bus, device, 0)) {
                continue;  // Device doesn't exist
            }
            
            // Check if it's a multi-function device
            uint8_t header_type = pci_read_config(bus, device, 0, PCI_CONFIG_HEADER_TYPE);
            if (header_type & 0x80) {
                functions = 8;  // Multi-function device
            }
            
            // Scan all functions
            for (uint8_t function = 0; function < functions; function++) {
                if (!pci_device_exists(bus, device, function)) {
                    continue;  // Function doesn't exist
                }
                
                if (pci_device_count >= MAX_PCI_DEVICES) {
                    kerror("PCI: Too many devices, stopping enumeration\n");
                    goto done;
                }
                
                pci_device_t* dev = &pci_devices[pci_device_count];
                dev->bus = bus;
                dev->device = device;
                dev->function = function;
                
                // Read device information
                uint32_t vendor_device = pci_read_config_dword(bus, device, function, PCI_CONFIG_VENDOR_ID);
                dev->vendor_id = vendor_device & 0xFFFF;
                dev->device_id = (vendor_device >> 16) & 0xFFFF;
                
                uint32_t class_rev = pci_read_config_dword(bus, device, function, PCI_CONFIG_REVISION_ID);
                // revision_id is not stored in pci_device_t structure
                dev->prog_if = (class_rev >> 8) & 0xFF;
                dev->subclass = (class_rev >> 16) & 0xFF;
                dev->class_code = (class_rev >> 24) & 0xFF;
                dev->header_type = pci_read_config(bus, device, function, PCI_CONFIG_HEADER_TYPE) & 0xFF;
                
                // Read BARs
                for (int i = 0; i < 6; i++) {
                    dev->bars[i] = pci_read_config_dword(bus, device, function, PCI_CONFIG_BAR0 + (i * 4));
                }
                
                // Read IRQ line and pin
                uint32_t irq_reg = pci_read_config_dword(bus, device, function, 0x3C);
                dev->irq_line = irq_reg & 0xFF;
                dev->irq_pin = (irq_reg >> 8) & 0xFF;
                
                kinfo("PCI: %02x:%02x.%x - Vendor: %04x Device: %04x Class: %02x:%02x:%02x IRQ: %u\n",
                      bus, device, function, dev->vendor_id, dev->device_id,
                      dev->class_code, dev->subclass, dev->prog_if, dev->irq_line);
                
                pci_device_count++;
            }
        }
    }
    
done:
    kinfo("PCI enumeration complete: %u device(s) found\n", pci_device_count);
    return ERR_OK;
}

/**
 * Initialize PCI subsystem
 */
error_code_t pci_init(void) {
    kinfo("Initializing PCI subsystem...\n");
    
    // Clear device list
    memset(pci_devices, 0, sizeof(pci_devices));
    pci_device_count = 0;
    
    // Enumerate devices
    return pci_enumerate();
}

/**
 * Find PCI device by vendor/device ID
 */
pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (uint32_t i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id &&
            pci_devices[i].device_id == device_id) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

/**
 * Find PCI device by class code
 */
pci_device_t* pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if) {
    for (uint32_t i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].class_code == class_code &&
            pci_devices[i].subclass == subclass &&
            (prog_if == 0xFF || pci_devices[i].prog_if == prog_if)) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

/**
 * Get device count
 */
uint32_t pci_get_device_count(void) {
    return pci_device_count;
}

/**
 * Get device by index
 */
pci_device_t* pci_get_device(uint32_t index) {
    if (index >= pci_device_count) {
        return NULL;
    }
    return &pci_devices[index];
}

/**
 * Decode BAR (Base Address Register) information
 */
error_code_t pci_decode_bar(pci_device_t* dev, uint8_t bar_index, pci_bar_info_t* info) {
    if (!dev || !info || bar_index >= 6) {
        return ERR_INVALID_PARAM;
    }
    
    uint32_t bar = (uint32_t)dev->bars[bar_index];
    
    // Check if BAR is unused
    if (bar == 0 || bar == 0xFFFFFFFF) {
        return ERR_NOT_FOUND;
    }
    
    // Determine BAR type
    if (bar & 0x1) {
        // I/O space BAR
        info->is_io = true;
        info->is_64bit = false;
        info->base_address = bar & 0xFFFFFFFC;  // 32-bit I/O address
        info->is_prefetchable = false;
    } else {
        // Memory space BAR
        info->is_io = false;
        info->is_64bit = (bar & 0x6) == 0x4;  // 64-bit if bits 2-3 = 010
        
        if (info->is_64bit && bar_index < 5) {
            // 64-bit BAR uses two consecutive BARs
            uint32_t bar_upper = (uint32_t)dev->bars[bar_index + 1];
            info->base_address = ((uint64_t)bar_upper << 32) | (bar & 0xFFFFFFF0);
        } else {
            info->base_address = bar & 0xFFFFFFF0;  // 32-bit memory address
        }
        
        info->is_prefetchable = (bar & 0x8) != 0;
    }
    
    // Calculate BAR size by writing all 1s and reading back
    uint8_t bus = dev->bus;
    uint8_t device = dev->device;
    uint8_t function = dev->function;
    uint8_t offset = PCI_CONFIG_BAR0 + (bar_index * 4);
    
    // Save original value
    uint32_t original = pci_read_config_dword(bus, device, function, offset);
    
    // Write all 1s
    pci_write_config_dword(bus, device, function, offset, 0xFFFFFFFF);
    
    // Read back
    uint32_t size_mask = pci_read_config_dword(bus, device, function, offset);
    
    // Restore original
    pci_write_config_dword(bus, device, function, offset, original);
    
    // Calculate size
    if (info->is_io) {
        size_mask &= 0xFFFFFFFC;
        info->size = (~size_mask) + 1;
    } else {
        size_mask &= 0xFFFFFFF0;
        if (info->is_64bit) {
            // For 64-bit, also check upper BAR
            if (bar_index < 5) {
                uint32_t original_upper = pci_read_config_dword(bus, device, function, offset + 4);
                pci_write_config_dword(bus, device, function, offset + 4, 0xFFFFFFFF);
                uint32_t size_mask_upper = pci_read_config_dword(bus, device, function, offset + 4);
                pci_write_config_dword(bus, device, function, offset + 4, original_upper);
                info->size = ((uint64_t)(~size_mask_upper) << 32) | ((~size_mask) + 1);
            } else {
                info->size = (~size_mask) + 1;
            }
        } else {
            info->size = (~size_mask) + 1;
        }
    }
    
    return ERR_OK;
}

/**
 * Get BAR size
 */
uint64_t pci_get_bar_size(pci_device_t* dev, uint8_t bar_index) {
    pci_bar_info_t info;
    if (pci_decode_bar(dev, bar_index, &info) == ERR_OK) {
        return info.size;
    }
    return 0;
}

/**
 * Check if device is PCI Express
 */
bool pci_is_pcie(pci_device_t* dev) {
    if (!dev) {
        return false;
    }
    
    // Check for PCI Express capability
    // PCIe devices have a capability list starting at offset 0x34
    uint8_t cap_ptr = pci_read_config(dev->bus, dev->device, dev->function, 0x34) & 0xFF;
    
    if (cap_ptr == 0 || cap_ptr == 0xFF) {
        return false;  // No capability list
    }
    
    // Walk capability list looking for PCIe capability (ID 0x10)
    uint8_t current = cap_ptr;
    int iterations = 0;
    
    while (current != 0 && iterations < 48) {  // Max 48 capabilities
        uint8_t cap_id = pci_read_config(dev->bus, dev->device, dev->function, current) & 0xFF;
        
        if (cap_id == 0x10) {
            return true;  // Found PCIe capability
        }
        
        // Move to next capability
        uint8_t next = pci_read_config(dev->bus, dev->device, dev->function, current + 1) & 0xFF;
        if (next == 0 || next == 0xFF) {
            break;
        }
        current = next;
        iterations++;
    }
    
    return false;
}

/**
 * Get IRQ line
 */
uint8_t pci_get_irq_line(pci_device_t* dev) {
    if (!dev) {
        return 0;
    }
    return dev->irq_line;
}

/**
 * Get IRQ pin
 */
uint8_t pci_get_irq_pin(pci_device_t* dev) {
    if (!dev) {
        return 0;
    }
    return dev->irq_pin;
}

