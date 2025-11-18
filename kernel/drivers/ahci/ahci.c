/**
 * @file ahci.c
 * @brief AHCI driver implementation
 * 
 * Implements SATA support via AHCI (Advanced Host Controller Interface).
 */

#include "../../include/types.h"
#include "../../include/errors.h"
#include "../../include/fs/block.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "../../include/string.h"
#include "../../include/mm/heap.h"
#include "../../include/mm/vmm.h"
#include "ahci.h"
#include "../pci/pci.h"

// Forward declarations
extern error_code_t block_device_register(block_device_t* device);

// AHCI controllers
static ahci_controller_t ahci_controllers[MAX_AHCI_DEVICES];
static uint32_t ahci_controller_count = 0;

// AHCI ports (per controller)
static ahci_port_t ahci_ports[MAX_AHCI_DEVICES * AHCI_MAX_PORTS];
static uint32_t ahci_port_count = 0;

/**
 * Read AHCI MMIO register (32-bit)
 */
static inline uint32_t ahci_read32(ahci_controller_t* ctrl, uint32_t offset) {
    return *(volatile uint32_t*)(ctrl->base_address + offset);
}

/**
 * Write AHCI MMIO register (32-bit)
 */
static inline void ahci_write32(ahci_controller_t* ctrl, uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(ctrl->base_address + offset) = value;
}

/**
 * Read AHCI port MMIO register (32-bit)
 */
static inline uint32_t ahci_port_read32(ahci_port_t* port, uint32_t offset) {
    uint32_t port_base = 0x100 + (port->port_num * 0x80);
    return ahci_read32(port->controller, port_base + offset);
}

/**
 * Write AHCI port MMIO register (32-bit)
 */
static inline void ahci_port_write32(ahci_port_t* port, uint32_t offset, uint32_t value) {
    uint32_t port_base = 0x100 + (port->port_num * 0x80);
    ahci_write32(port->controller, port_base + offset, value);
}

/**
 * Wait for port to be ready
 */
static error_code_t ahci_wait_ready(ahci_port_t* port) {
    int timeout = 100000;
    
    while (timeout-- > 0) {
        uint32_t cmd = ahci_port_read32(port, AHCI_PxCMD);
        if (!(cmd & AHCI_PxCMD_ST) && !(cmd & AHCI_PxCMD_CCS_MASK)) {
            return ERR_OK;
        }
    }
    
    return ERR_TIMEOUT;
}

/**
 * Identify AHCI device
 */
static error_code_t ahci_identify(ahci_port_t* port) {
    // TODO: Implement full AHCI identify command
    // This requires setting up command lists, FIS structures, and PRDT
    
    // For now, placeholder
    kinfo("AHCI: Port %u identify (placeholder)\n", port->port_num);
    
    // Set default values
    port->lba48 = true;
    port->sectors = 0;  // Will be set by identify
    port->sector_size = 512;
    strcpy(port->model, "AHCI Device");
    
    return ERR_OK;
}

/**
 * Block device read callback for AHCI
 */
static error_code_t ahci_block_read(block_device_t* dev, uint64_t block_num, void* buffer) {
    ahci_port_t* port = (ahci_port_t*)dev->private_data;
    if (!port || !port->present) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    return ahci_read_sectors(port, block_num, 1, buffer);
}

/**
 * Block device write callback for AHCI
 */
static error_code_t ahci_block_write(block_device_t* dev, uint64_t block_num, const void* buffer) {
    ahci_port_t* port = (ahci_port_t*)dev->private_data;
    if (!port || !port->present) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    return ahci_write_sectors(port, block_num, 1, buffer);
}

/**
 * Block device read blocks callback for AHCI
 */
static error_code_t ahci_block_read_blocks(block_device_t* dev, uint64_t start_block, uint64_t count, void* buffer) {
    ahci_port_t* port = (ahci_port_t*)dev->private_data;
    if (!port || !port->present) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    // Read in chunks
    uint8_t* buf = (uint8_t*)buffer;
    for (uint64_t i = 0; i < count; i++) {
        error_code_t err = ahci_read_sectors(port, start_block + i, 1, buf + (i * 512));
        if (err != ERR_OK) {
            return err;
        }
    }
    
    return ERR_OK;
}

/**
 * Block device write blocks callback for AHCI
 */
static error_code_t ahci_block_write_blocks(block_device_t* dev, uint64_t start_block, uint64_t count, const void* buffer) {
    ahci_port_t* port = (ahci_port_t*)dev->private_data;
    if (!port || !port->present) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    // Write in chunks
    const uint8_t* buf = (const uint8_t*)buffer;
    for (uint64_t i = 0; i < count; i++) {
        error_code_t err = ahci_write_sectors(port, start_block + i, 1, buf + (i * 512));
        if (err != ERR_OK) {
            return err;
        }
    }
    
    return ERR_OK;
}

/**
 * Initialize AHCI driver
 */
error_code_t ahci_init(void) {
    kinfo("Initializing AHCI driver...\n");
    
    // Clear controllers
    memset(ahci_controllers, 0, sizeof(ahci_controllers));
    ahci_controller_count = 0;
    memset(ahci_ports, 0, sizeof(ahci_ports));
    ahci_port_count = 0;
    
    // TODO: Detect AHCI controllers via PCI
    // For now, placeholder
    kinfo("AHCI: PCI enumeration not yet implemented\n");
    
    return ERR_OK;
}

/**
 * Detect AHCI controllers
 */
error_code_t ahci_detect_controllers(void) {
    kinfo("Detecting AHCI controllers...\n");
    
    // Find AHCI controllers via PCI
    uint32_t controller_idx = 0;
    
    for (uint32_t i = 0; i < pci_get_device_count() && controller_idx < MAX_AHCI_DEVICES; i++) {
        pci_device_t* dev = pci_get_device(i);
        if (!dev) continue;
        
        // Check if it's an AHCI controller
        if (dev->class_code != PCI_CLASS_MASS_STORAGE ||
            dev->subclass != PCI_SUBCLASS_SATA ||
            dev->prog_if != PCI_PROG_IF_AHCI) {
            continue;
        }
        
        kinfo("AHCI: Found controller at %02x:%02x.%x\n",
              dev->bus, dev->device, dev->function);
        
        // Get MMIO base address from BAR5
        uint64_t mmio_base = dev->bars[5] & ~0xFFF;  // Clear lower 12 bits (4KB aligned)
        if (mmio_base == 0 || mmio_base == 0xFFFFFFFF) {
            kerror("AHCI: Invalid MMIO base address\n");
            continue;
        }
        
        // Map MMIO region (simplified - assume identity mapped for now)
        ahci_controller_t* ctrl = &ahci_controllers[controller_idx];
        ctrl->base_address = mmio_base;
        ctrl->capabilities = ahci_read32(ctrl, AHCI_CAP);
        ctrl->num_ports = ((ctrl->capabilities & AHCI_CAP_NP_MASK) >> AHCI_CAP_NP_SHIFT) + 1;
        ctrl->present = true;
        
        kinfo("AHCI: Controller %u - MMIO: 0x%llx, Ports: %u\n",
              controller_idx, mmio_base, ctrl->num_ports);
        
        // TODO: Initialize ports and detect devices
        // For now, just mark controller as present
        
        controller_idx++;
        ahci_controller_count = controller_idx;
    }
    
    kinfo("AHCI: Found %u controller(s)\n", ahci_controller_count);
    return ERR_OK;
}

/**
 * Get AHCI controller by index
 */
ahci_controller_t* ahci_get_controller(uint32_t index) {
    if (index >= ahci_controller_count) {
        return NULL;
    }
    return &ahci_controllers[index];
}

/**
 * Get AHCI port
 */
ahci_port_t* ahci_get_port(uint32_t controller_index, uint32_t port_index) {
    if (controller_index >= ahci_controller_count) {
        return NULL;
    }
    
    // Find port
    for (uint32_t i = 0; i < ahci_port_count; i++) {
        if (ahci_ports[i].controller == &ahci_controllers[controller_index] &&
            ahci_ports[i].port_num == port_index) {
            return &ahci_ports[i];
        }
    }
    
    return NULL;
}

/**
 * Read sectors from AHCI port
 */
error_code_t ahci_read_sectors(ahci_port_t* port, uint64_t lba, uint32_t count, void* buffer) {
    if (!port || !port->present || !buffer) {
        return ERR_INVALID_ARG;
    }
    
    // TODO: Implement full AHCI read command
    // This requires:
    // 1. Setting up command list entry
    // 2. Setting up command FIS (FIS_TYPE_REG_H2D)
    // 3. Setting up PRDT (Physical Region Descriptor Table)
    // 4. Issuing command via PxCI
    // 5. Waiting for completion
    // 6. Handling interrupts
    
    kinfo("AHCI: Read sectors (placeholder) - LBA %llu, count %u\n", lba, count);
    return ERR_NOT_SUPPORTED;
}

/**
 * Write sectors to AHCI port
 */
error_code_t ahci_write_sectors(ahci_port_t* port, uint64_t lba, uint32_t count, const void* buffer) {
    if (!port || !port->present || !buffer) {
        return ERR_INVALID_ARG;
    }
    
    // TODO: Implement full AHCI write command
    kinfo("AHCI: Write sectors (placeholder) - LBA %llu, count %u\n", lba, count);
    return ERR_NOT_SUPPORTED;
}

