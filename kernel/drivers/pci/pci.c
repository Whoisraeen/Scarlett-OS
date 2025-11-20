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
                
                kinfo("PCI: %02x:%02x.%x - Vendor: %04x Device: %04x Class: %02x:%02x:%02x\n",
                      bus, device, function, dev->vendor_id, dev->device_id,
                      dev->class_code, dev->subclass, dev->prog_if);
                
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

