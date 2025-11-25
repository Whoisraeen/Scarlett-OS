/**
 * @file ethernet.c
 * @brief Ethernet NIC driver implementation
 */

#include "../../include/drivers/ethernet.h"
#include "../../include/net/network.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "../../include/mm/heap.h"
#include "../../include/mm/vmm.h"
#include "../../include/mm/pmm.h"  // For PAGE_SIZE
#include "../../include/string.h"
#include "../../include/sync/spinlock.h"
#include "../pci/pci.h"

// Registered Ethernet NICs
#define MAX_ETHERNET_NICS 8
static ethernet_nic_t ethernet_nics[MAX_ETHERNET_NICS];
static uint32_t ethernet_nic_count = 0;
static spinlock_t ethernet_lock;
static bool driver_initialized = false;

/**
 * Send packet wrapper (converts net_device_t* to ethernet_nic_t*)
 */
static error_code_t ethernet_send_wrapper(net_device_t* dev, void* data, size_t len) {
    if (!dev || !dev->driver_data) {
        return ERR_INVALID_ARG;
    }
    ethernet_nic_t* nic = (ethernet_nic_t*)dev->driver_data;
    return nic->send_packet(nic, data, len);
}

/**
 * Receive packet wrapper (converts net_device_t* to ethernet_nic_t*)
 */
static error_code_t ethernet_receive_wrapper(net_device_t* dev, void* buffer, size_t* len) {
    if (!dev || !dev->driver_data) {
        return ERR_INVALID_ARG;
    }
    ethernet_nic_t* nic = (ethernet_nic_t*)dev->driver_data;
    return nic->receive_packet(nic, buffer, len);
}

/**
 * Generic send packet (software implementation)
 */
static error_code_t ethernet_sw_send_packet(ethernet_nic_t* nic, void* data, size_t len) {
    if (!nic || !data || len == 0) {
        return ERR_INVALID_ARG;
    }
    
    // In a real driver, this would write to the NIC's transmit queue
    // For now, this is a placeholder that would interface with hardware
    kinfo("Ethernet: Sending packet (%zu bytes) via NIC\n", len);
    
    return ERR_OK;
}

/**
 * Generic receive packet (software implementation)
 */
static error_code_t ethernet_sw_receive_packet(ethernet_nic_t* nic, void* buffer, size_t* len) {
    if (!nic || !buffer || !len) {
        return ERR_INVALID_ARG;
    }
    
    // In a real driver, this would read from the NIC's receive queue
    // For now, this is a placeholder that would interface with hardware
    return ERR_NOT_FOUND;  // No packet available
}

/**
 * Get MAC address from PCI device (generic)
 */
static error_code_t ethernet_get_mac_from_pci(ethernet_nic_t* nic, uint8_t* mac) {
    if (!nic || !mac || !nic->pci_dev) {
        return ERR_INVALID_ARG;
    }
    
    // Read MAC address from PCI BAR
    // Many NICs store MAC address in BAR0 or EEPROM
    // For now, generate a pseudo MAC based on PCI device ID
    
    uint16_t vendor = nic->pci_dev->vendor_id;
    uint16_t device = nic->pci_dev->device_id;
    
    // Generate deterministic MAC from PCI IDs
    mac[0] = 0x02;  // Locally administered
    mac[1] = (vendor >> 8) & 0xFF;
    mac[2] = vendor & 0xFF;
    mac[3] = (device >> 8) & 0xFF;
    mac[4] = device & 0xFF;
    mac[5] = (ethernet_nic_count + 1) & 0xFF;
    
    return ERR_OK;
}

/**
 * Initialize Ethernet NIC
 */
static error_code_t ethernet_nic_init(ethernet_nic_t* nic) {
    if (!nic || !nic->pci_dev) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("Initializing Ethernet NIC (Vendor: 0x%04x, Device: 0x%04x)\n",
          nic->pci_dev->vendor_id, nic->pci_dev->device_id);
    
    // Decode BAR0 to get MMIO base address
    extern error_code_t pci_decode_bar(pci_device_t* dev, uint8_t bar_index, pci_bar_info_t* info);
    pci_bar_info_t bar_info;
    
    if (pci_decode_bar(nic->pci_dev, 0, &bar_info) != ERR_OK) {
        kwarn("Ethernet: Failed to decode BAR0\n");
        return ERR_INVALID_STATE;
    }
    
    if (bar_info.is_io) {
        // I/O port BAR - not supported yet
        kwarn("Ethernet: I/O port BAR not supported\n");
        return ERR_NOT_SUPPORTED;
    }
    
    if (bar_info.base_address == 0) {
        kwarn("Ethernet: No MMIO address in BAR0\n");
        return ERR_INVALID_STATE;
    }
    
    // Map MMIO to virtual address with proper VMM mapping and MMIO flags
    // DONE: VMM mapping with MMIO flags implemented
    // Use kernel address space (NULL means kernel address space)
    address_space_t* kernel_as = NULL;  // NULL means kernel address space
    
    // Use MMIO region in kernel space (0xFFFF800000000000 + physical address)
    vaddr_t mmio_vaddr = 0xFFFF800000000000ULL + bar_info.base_address;
    
    // Map with MMIO flags: NOCACHE (device memory), WRITETHROUGH (no write buffering)
    uint64_t mmio_flags = VMM_PRESENT | VMM_WRITE | VMM_NOCACHE | VMM_WRITETHROUGH;
    
    // Map each page of the MMIO region
    size_t mmio_pages = (bar_info.size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < mmio_pages; i++) {
        paddr_t page_paddr = bar_info.base_address + (i * PAGE_SIZE);
        vaddr_t page_vaddr = mmio_vaddr + (i * PAGE_SIZE);
        
        if (vmm_map_page(kernel_as, page_vaddr, page_paddr, mmio_flags) != 0) {
            kerror("Ethernet: Failed to map MMIO page %lu\n", i);
            return ERR_OUT_OF_MEMORY;
        }
    }
    
    nic->mmio_base = (void*)mmio_vaddr;
    
    kinfo("Ethernet: MMIO base: 0x%llx, size: 0x%llx\n", 
          bar_info.base_address, bar_info.size);
    
    // Get MAC address
    error_code_t err = ethernet_get_mac_from_pci(nic, nic->mac_address);
    if (err != ERR_OK) {
        return err;
    }
    
    // Set up driver functions
    nic->send_packet = ethernet_sw_send_packet;
    nic->receive_packet = ethernet_sw_receive_packet;
    nic->get_mac_address = ethernet_get_mac_from_pci;
    
    // Initialize network device structure
    memset(&nic->net_device, 0, sizeof(net_device_t));
    // Format device name
    char name_buf[16];
    name_buf[0] = 'e';
    name_buf[1] = 't';
    name_buf[2] = 'h';
    name_buf[3] = '0' + (ethernet_nic_count % 10);
    name_buf[4] = '\0';
    strncpy(nic->net_device.name, name_buf, sizeof(nic->net_device.name) - 1);
    nic->net_device.name[sizeof(nic->net_device.name) - 1] = '\0';
    nic->net_device.type = NET_DEVICE_TYPE_ETHERNET;
    memcpy(nic->net_device.mac_address, nic->mac_address, 6);
    nic->net_device.up = false;
    nic->net_device.driver_data = nic;
    // Set up network device callbacks
    nic->net_device.send_packet = ethernet_send_wrapper;
    nic->net_device.receive_packet = ethernet_receive_wrapper;
    
    nic->initialized = true;
    
    kinfo("Ethernet NIC initialized: %s (MAC: %02x:%02x:%02x:%02x:%02x:%02x)\n",
          nic->net_device.name,
          nic->mac_address[0], nic->mac_address[1], nic->mac_address[2],
          nic->mac_address[3], nic->mac_address[4], nic->mac_address[5]);
    
    return ERR_OK;
}

/**
 * Probe PCI device to see if it's an Ethernet NIC
 */
error_code_t ethernet_driver_probe(pci_device_t* pci_dev) {
    if (!pci_dev) {
        return ERR_INVALID_ARG;
    }
    
    // Check if it's a network controller
    if (pci_dev->class_code != 0x02 || pci_dev->subclass != 0x00) {
        return ERR_NOT_FOUND;  // Not an Ethernet controller
    }
    
    // Check if we have space
    if (ethernet_nic_count >= MAX_ETHERNET_NICS) {
        return ERR_OUT_OF_MEMORY;
    }
    
    spinlock_lock(&ethernet_lock);
    
    // Allocate NIC structure
    ethernet_nic_t* nic = &ethernet_nics[ethernet_nic_count];
    memset(nic, 0, sizeof(ethernet_nic_t));
    nic->pci_dev = pci_dev;
    
    // Initialize NIC
    error_code_t err = ethernet_nic_init(nic);
    if (err != ERR_OK) {
        spinlock_unlock(&ethernet_lock);
        return err;
    }
    
    // Register with network stack
    err = network_register_device(&nic->net_device);
    if (err != ERR_OK) {
        spinlock_unlock(&ethernet_lock);
        return err;
    }
    
    ethernet_nic_count++;
    
    spinlock_unlock(&ethernet_lock);
    
    kinfo("Ethernet driver: Probing successful for %02x:%02x.%x\n",
          pci_dev->bus, pci_dev->device, pci_dev->function);
    
    return ERR_OK;
}

/**
 * Initialize Ethernet driver
 */
error_code_t ethernet_driver_init(void) {
    if (driver_initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing Ethernet driver...\n");
    
    memset(ethernet_nics, 0, sizeof(ethernet_nics));
    ethernet_nic_count = 0;
    spinlock_init(&ethernet_lock);
    driver_initialized = true;
    
    // Scan PCI bus for Ethernet controllers
    extern uint32_t pci_get_device_count(void);
    extern pci_device_t* pci_get_device(uint32_t index);
    
    uint32_t device_count = pci_get_device_count();
    kinfo("Scanning %u PCI devices for Ethernet controllers...\n", device_count);
    
    uint32_t found_count = 0;
    for (uint32_t i = 0; i < device_count; i++) {
        pci_device_t* dev = pci_get_device(i);
        if (!dev) {
            continue;
        }
        
        error_code_t err = ethernet_driver_probe(dev);
        if (err == ERR_OK) {
            found_count++;
        }
    }
    
    kinfo("Ethernet driver initialized: Found %u NIC(s)\n", found_count);
    
    return ERR_OK;
}

/**
 * Register Ethernet NIC (external)
 */
error_code_t ethernet_nic_register(ethernet_nic_t* nic) {
    if (!nic) {
        return ERR_INVALID_ARG;
    }
    
    spinlock_lock(&ethernet_lock);
    
    if (ethernet_nic_count >= MAX_ETHERNET_NICS) {
        spinlock_unlock(&ethernet_lock);
        return ERR_OUT_OF_MEMORY;
    }
    
    // Copy NIC structure
    memcpy(&ethernet_nics[ethernet_nic_count], nic, sizeof(ethernet_nic_t));
    ethernet_nic_count++;
    
    // Register with network stack
    error_code_t err = network_register_device(&nic->net_device);
    
    spinlock_unlock(&ethernet_lock);
    
    return err;
}

/**
 * Find NIC by MAC address
 */
ethernet_nic_t* ethernet_nic_find_by_mac(uint8_t* mac) {
    if (!mac) {
        return NULL;
    }
    
    spinlock_lock(&ethernet_lock);
    
    for (uint32_t i = 0; i < ethernet_nic_count; i++) {
        if (memcmp(ethernet_nics[i].mac_address, mac, 6) == 0) {
            ethernet_nic_t* nic = &ethernet_nics[i];
            spinlock_unlock(&ethernet_lock);
            return nic;
        }
    }
    
    spinlock_unlock(&ethernet_lock);
    return NULL;
}

/**
 * Get default Ethernet NIC
 */
ethernet_nic_t* ethernet_nic_get_default(void) {
    spinlock_lock(&ethernet_lock);
    
    if (ethernet_nic_count == 0) {
        spinlock_unlock(&ethernet_lock);
        return NULL;
    }
    
    ethernet_nic_t* nic = &ethernet_nics[0];
    spinlock_unlock(&ethernet_lock);
    return nic;
}

