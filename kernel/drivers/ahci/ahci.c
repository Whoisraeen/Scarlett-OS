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

// Forward declaration
extern address_space_t* vmm_get_kernel_address_space(void);

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
    
    // Ensure PCI is enumerated first
    extern error_code_t pci_enumerate(void);
    pci_enumerate();
    
    // Detect AHCI controllers via PCI
    return ahci_detect_controllers();
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
        
        // Decode BAR5 (AHCI MMIO base address)
        // Note: AHCI typically uses BAR5, but let's check BAR0 first (more common)
        pci_bar_info_t bar_info;
        error_code_t bar_result = ERR_NOT_FOUND;
        
        // Try BAR0 first (most common)
        if (pci_decode_bar(dev, 0, &bar_info) == ERR_OK && !bar_info.is_io) {
            bar_result = ERR_OK;
        } else if (pci_decode_bar(dev, 5, &bar_info) == ERR_OK && !bar_info.is_io) {
            // Fall back to BAR5
            bar_result = ERR_OK;
        }
        
        if (bar_result != ERR_OK || bar_info.base_address == 0) {
            kerror("AHCI: Failed to find valid MMIO BAR\n");
            continue;
        }
        
        // Map MMIO region to kernel address space
        // For now, assume identity mapping (will be improved with proper VMM mapping later)
        ahci_controller_t* ctrl = &ahci_controllers[controller_idx];
        ctrl->base_address = bar_info.base_address;
        ctrl->capabilities = ahci_read32(ctrl, AHCI_CAP);
        ctrl->num_ports = ((ctrl->capabilities & AHCI_CAP_NP_MASK) >> AHCI_CAP_NP_SHIFT) + 1;
        ctrl->present = true;
        
        kinfo("AHCI: Controller %u - MMIO: 0x%llx, Size: 0x%llx, Ports: %u\n",
              controller_idx, bar_info.base_address, bar_info.size, ctrl->num_ports);
        
        // Enable AHCI mode in GHC register
        uint32_t ghc = ahci_read32(ctrl, AHCI_GHC);
        if (!(ghc & (1 << 31))) {  // Check if AHCI is already enabled
            ahci_write32(ctrl, AHCI_GHC, ghc | (1 << 31));  // Enable AHCI
            kinfo("AHCI: Enabled AHCI mode\n");
        }
        
        // TODO: Initialize ports and detect devices
        // This requires setting up command lists, FIS, and port initialization
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
    
    // Allocate command list (1KB aligned, 1KB size)
    // Command list: 32 entries * 32 bytes = 1024 bytes
    ahci_cmd_header_t* cmd_list = (ahci_cmd_header_t*)kmalloc(1024);
    if (!cmd_list) {
        return ERR_OUT_OF_MEMORY;
    }
    memset(cmd_list, 0, 1024);
    
    // Allocate FIS receive area (256-byte aligned, 256 bytes)
    uint8_t* fis_base = (uint8_t*)kmalloc(256);
    if (!fis_base) {
        kfree(cmd_list);
        return ERR_OUT_OF_MEMORY;
    }
    memset(fis_base, 0, 256);
    
    // Allocate command table (128 bytes + PRDT)
    size_t cmd_table_size = sizeof(ahci_cmd_table_t) + sizeof(ahci_prdt_entry_t);
    ahci_cmd_table_t* cmd_table = (ahci_cmd_table_t*)kmalloc(cmd_table_size);
    if (!cmd_table) {
        kfree(cmd_list);
        kfree(fis_base);
        return ERR_OUT_OF_MEMORY;
    }
    memset(cmd_table, 0, cmd_table_size);
    
    // Get physical addresses using VMM
    address_space_t* kernel_as = vmm_get_kernel_address_space();
    uint64_t cmd_list_phys = vmm_get_physical(kernel_as, (vaddr_t)cmd_list);
    uint64_t fis_base_phys = vmm_get_physical(kernel_as, (vaddr_t)fis_base);
    uint64_t cmd_table_phys = vmm_get_physical(kernel_as, (vaddr_t)cmd_table);
    
    if (!cmd_list_phys || !fis_base_phys || !cmd_table_phys) {
        kerror("AHCI: Failed to get physical addresses\n");
        kfree(cmd_list);
        kfree(fis_base);
        kfree(cmd_table);
        return ERR_MAPPING_FAILED;
    }
    
    // Set up command list entry (slot 0)
    ahci_cmd_header_t* cmd_header = &cmd_list[0];
    cmd_header->flags = (5 << 0) | (1 << 6);  // CFL=5 (20 bytes), Write=0 (read)
    cmd_header->prdtl = 1;  // One PRDT entry
    cmd_header->ctba = cmd_table_phys;
    
    // Set up Command FIS (Host to Device)
    ahci_fis_h2d_t* fis = (ahci_fis_h2d_t*)cmd_table->cfis;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->pmport_c = 0x80;  // Command bit set
    fis->command = port->lba48 ? 0x25 : 0x20;  // READ DMA EXT or READ DMA
    fis->device = 0x40;  // LBA mode
    
    if (port->lba48) {
        // 48-bit LBA
        fis->lba_low = lba & 0xFF;
        fis->lba_mid = (lba >> 8) & 0xFF;
        fis->lba_high = (lba >> 16) & 0xFF;
        fis->device = 0x40 | ((lba >> 24) & 0x0F);
        fis->lba_low_ext = (lba >> 24) & 0xFF;
        fis->lba_mid_ext = (lba >> 32) & 0xFF;
        fis->lba_high_ext = (lba >> 40) & 0xFF;
        fis->count_low = count & 0xFF;
        fis->count_high = (count >> 8) & 0xFF;
    } else {
        // 28-bit LBA
        fis->lba_low = lba & 0xFF;
        fis->lba_mid = (lba >> 8) & 0xFF;
        fis->lba_high = (lba >> 16) & 0xFF;
        fis->device = 0x40 | ((lba >> 24) & 0x0F);
        fis->count_low = count & 0xFF;
        fis->count_high = (count >> 8) & 0xFF;
    }
    
    // Set up PRDT entry
    ahci_prdt_entry_t* prdt = (ahci_prdt_entry_t*)((uint8_t*)cmd_table + sizeof(ahci_cmd_table_t));
    uint64_t buffer_phys = vmm_get_physical(kernel_as, (vaddr_t)buffer);
    if (!buffer_phys) {
        kerror("AHCI: Failed to get physical address for buffer\n");
        kfree(cmd_list);
        kfree(fis_base);
        kfree(cmd_table);
        return ERR_MAPPING_FAILED;
    }
    prdt->dba = buffer_phys;  // Data buffer physical address
    prdt->dbc = (count * 512) - 1;  // Byte count (minus 1)
    
    // Program port registers
    ahci_port_write32(port, AHCI_PxCLB, cmd_list_phys & 0xFFFFFFFF);
    ahci_port_write32(port, AHCI_PxCLBU, (cmd_list_phys >> 32) & 0xFFFFFFFF);
    ahci_port_write32(port, AHCI_PxFB, fis_base_phys & 0xFFFFFFFF);
    ahci_port_write32(port, AHCI_PxFBU, (fis_base_phys >> 32) & 0xFFFFFFFF);
    
    // Start command engine
    uint32_t cmd = ahci_port_read32(port, AHCI_PxCMD);
    if (!(cmd & AHCI_PxCMD_ST)) {
        ahci_port_write32(port, AHCI_PxCMD, cmd | AHCI_PxCMD_ST);
    }
    if (!(cmd & AHCI_PxCMD_FRE)) {
        ahci_port_write32(port, AHCI_PxCMD, cmd | AHCI_PxCMD_FRE);
    }
    
    // Issue command (set bit 0 in PxCI)
    ahci_port_write32(port, AHCI_PxCI, 1);
    
    // Wait for completion (poll PxCI bit 0)
    int timeout = 1000000;
    while (timeout-- > 0) {
        uint32_t ci = ahci_port_read32(port, AHCI_PxCI);
        if (!(ci & 1)) {
            // Command completed
            break;
        }
    }
    
    if (timeout <= 0) {
        kerror("AHCI: Read command timeout\n");
        kfree(cmd_list);
        kfree(fis_base);
        kfree(cmd_table);
        return ERR_TIMEOUT;
    }
    
    // Check for errors
    uint32_t tfd = ahci_port_read32(port, AHCI_PxTFD);
    if (tfd & 0x01) {  // Error bit
        kerror("AHCI: Read command error (TFD=0x%x)\n", tfd);
        kfree(cmd_list);
        kfree(fis_base);
        kfree(cmd_table);
        return ERR_IO_ERROR;
    }
    
    // Cleanup
    kfree(cmd_list);
    kfree(fis_base);
    kfree(cmd_table);
    
    return ERR_OK;
}

/**
 * Write sectors to AHCI port
 */
error_code_t ahci_write_sectors(ahci_port_t* port, uint64_t lba, uint32_t count, const void* buffer) {
    if (!port || !port->present || !buffer) {
        return ERR_INVALID_ARG;
    }
    
    // Similar to read, but with write command
    // Allocate command list (1KB aligned, 1KB size)
    ahci_cmd_header_t* cmd_list = (ahci_cmd_header_t*)kmalloc(1024);
    if (!cmd_list) {
        return ERR_OUT_OF_MEMORY;
    }
    memset(cmd_list, 0, 1024);
    
    // Allocate FIS receive area (256-byte aligned, 256 bytes)
    uint8_t* fis_base = (uint8_t*)kmalloc(256);
    if (!fis_base) {
        kfree(cmd_list);
        return ERR_OUT_OF_MEMORY;
    }
    memset(fis_base, 0, 256);
    
    // Allocate command table (128 bytes + PRDT)
    size_t cmd_table_size = sizeof(ahci_cmd_table_t) + sizeof(ahci_prdt_entry_t);
    ahci_cmd_table_t* cmd_table = (ahci_cmd_table_t*)kmalloc(cmd_table_size);
    if (!cmd_table) {
        kfree(cmd_list);
        kfree(fis_base);
        return ERR_OUT_OF_MEMORY;
    }
    memset(cmd_table, 0, cmd_table_size);
    
    // Get physical addresses using VMM
    address_space_t* kernel_as = vmm_get_kernel_address_space();
    uint64_t cmd_list_phys = vmm_get_physical(kernel_as, (vaddr_t)cmd_list);
    uint64_t fis_base_phys = vmm_get_physical(kernel_as, (vaddr_t)fis_base);
    uint64_t cmd_table_phys = vmm_get_physical(kernel_as, (vaddr_t)cmd_table);
    
    if (!cmd_list_phys || !fis_base_phys || !cmd_table_phys) {
        kerror("AHCI: Failed to get physical addresses\n");
        kfree(cmd_list);
        kfree(fis_base);
        kfree(cmd_table);
        return ERR_MAPPING_FAILED;
    }
    
    // Set up command list entry (slot 0)
    ahci_cmd_header_t* cmd_header = &cmd_list[0];
    cmd_header->flags = (5 << 0) | (1 << 6) | (1 << 5);  // CFL=5, Write=1, ATAPI=0
    cmd_header->prdtl = 1;
    cmd_header->ctba = cmd_table_phys;
    
    // Set up Command FIS (Host to Device) - Write command
    ahci_fis_h2d_t* fis = (ahci_fis_h2d_t*)cmd_table->cfis;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->pmport_c = 0x80;  // Command bit set
    fis->command = port->lba48 ? 0x35 : 0x30;  // WRITE DMA EXT or WRITE DMA
    fis->device = 0x40;  // LBA mode
    
    if (port->lba48) {
        // 48-bit LBA
        fis->lba_low = lba & 0xFF;
        fis->lba_mid = (lba >> 8) & 0xFF;
        fis->lba_high = (lba >> 16) & 0xFF;
        fis->device = 0x40 | ((lba >> 24) & 0x0F);
        fis->lba_low_ext = (lba >> 24) & 0xFF;
        fis->lba_mid_ext = (lba >> 32) & 0xFF;
        fis->lba_high_ext = (lba >> 40) & 0xFF;
        fis->count_low = count & 0xFF;
        fis->count_high = (count >> 8) & 0xFF;
    } else {
        // 28-bit LBA
        fis->lba_low = lba & 0xFF;
        fis->lba_mid = (lba >> 8) & 0xFF;
        fis->lba_high = (lba >> 16) & 0xFF;
        fis->device = 0x40 | ((lba >> 24) & 0x0F);
        fis->count_low = count & 0xFF;
        fis->count_high = (count >> 8) & 0xFF;
    }
    
    // Set up PRDT entry
    ahci_prdt_entry_t* prdt = (ahci_prdt_entry_t*)((uint8_t*)cmd_table + sizeof(ahci_cmd_table_t));
    uint64_t buffer_phys = vmm_get_physical(kernel_as, (vaddr_t)buffer);
    if (!buffer_phys) {
        kerror("AHCI: Failed to get physical address for buffer\n");
        kfree(cmd_list);
        kfree(fis_base);
        kfree(cmd_table);
        return ERR_MAPPING_FAILED;
    }
    prdt->dba = buffer_phys;  // Data buffer physical address
    prdt->dbc = (count * 512) - 1;  // Byte count (minus 1)
    
    // Program port registers
    ahci_port_write32(port, AHCI_PxCLB, cmd_list_phys & 0xFFFFFFFF);
    ahci_port_write32(port, AHCI_PxCLBU, (cmd_list_phys >> 32) & 0xFFFFFFFF);
    ahci_port_write32(port, AHCI_PxFB, fis_base_phys & 0xFFFFFFFF);
    ahci_port_write32(port, AHCI_PxFBU, (fis_base_phys >> 32) & 0xFFFFFFFF);
    
    // Start command engine
    uint32_t cmd = ahci_port_read32(port, AHCI_PxCMD);
    if (!(cmd & AHCI_PxCMD_ST)) {
        ahci_port_write32(port, AHCI_PxCMD, cmd | AHCI_PxCMD_ST);
    }
    if (!(cmd & AHCI_PxCMD_FRE)) {
        ahci_port_write32(port, AHCI_PxCMD, cmd | AHCI_PxCMD_FRE);
    }
    
    // Issue command (set bit 0 in PxCI)
    ahci_port_write32(port, AHCI_PxCI, 1);
    
    // Wait for completion
    int timeout = 1000000;
    while (timeout-- > 0) {
        uint32_t ci = ahci_port_read32(port, AHCI_PxCI);
        if (!(ci & 1)) {
            break;
        }
    }
    
    if (timeout <= 0) {
        kerror("AHCI: Write command timeout\n");
        kfree(cmd_list);
        kfree(fis_base);
        kfree(cmd_table);
        return ERR_TIMEOUT;
    }
    
    // Check for errors
    uint32_t tfd = ahci_port_read32(port, AHCI_PxTFD);
    if (tfd & 0x01) {  // Error bit
        kerror("AHCI: Write command error (TFD=0x%x)\n", tfd);
        kfree(cmd_list);
        kfree(fis_base);
        kfree(cmd_table);
        return ERR_IO_ERROR;
    }
    
    // Cleanup
    kfree(cmd_list);
    kfree(fis_base);
    kfree(cmd_table);
    
    return ERR_OK;
}

