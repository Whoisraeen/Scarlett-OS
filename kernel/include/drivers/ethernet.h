/**
 * @file ethernet.h
 * @brief Ethernet NIC driver interface
 */

#ifndef KERNEL_DRIVERS_ETHERNET_H
#define KERNEL_DRIVERS_ETHERNET_H

#include "types.h"
#include "errors.h"
#include "../../drivers/pci/pci.h"
#include "../net/network.h"

// Ethernet NIC structure
typedef struct {
    pci_device_t* pci_dev;        // PCI device
    void* mmio_base;               // MMIO base address
    uint8_t mac_address[6];        // MAC address
    uint32_t ip_address;           // IP address
    uint32_t netmask;              // Network mask
    uint32_t gateway;              // Gateway address
    bool up;                       // Interface up?
    bool initialized;              // Driver initialized?
    
    // Driver functions
    error_code_t (*send_packet)(struct ethernet_nic* nic, void* data, size_t len);
    error_code_t (*receive_packet)(struct ethernet_nic* nic, void* buffer, size_t* len);
    error_code_t (*get_mac_address)(struct ethernet_nic* nic, uint8_t* mac);
    
    net_device_t net_device;       // Network device structure
} ethernet_nic_t;

// Ethernet driver functions
error_code_t ethernet_driver_init(void);
error_code_t ethernet_driver_probe(pci_device_t* pci_dev);
error_code_t ethernet_nic_register(ethernet_nic_t* nic);
ethernet_nic_t* ethernet_nic_find_by_mac(uint8_t* mac);
ethernet_nic_t* ethernet_nic_get_default(void);

#endif // KERNEL_DRIVERS_ETHERNET_H

