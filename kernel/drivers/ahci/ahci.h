/**
 * @file ahci.h
 * @brief AHCI (Advanced Host Controller Interface) driver
 * 
 * Provides SATA (Serial ATA) support via AHCI controller.
 */

#ifndef KERNEL_DRIVERS_AHCI_H
#define KERNEL_DRIVERS_AHCI_H

#include "../../include/types.h"
#include "../../include/errors.h"
#include "../../include/fs/block.h"

// AHCI register offsets
#define AHCI_CAP        0x00    // Host Capabilities
#define AHCI_GHC        0x04    // Global Host Control
#define AHCI_IS         0x08    // Interrupt Status
#define AHCI_PI         0x0C    // Ports Implemented
#define AHCI_VS         0x10    // Version
#define AHCI_CCC_CTL    0x14    // Command Completion Coalescing Control
#define AHCI_CCC_PORTS  0x18    // Command Completion Coalescing Ports
#define AHCI_EM_LOC     0x1C    // Enclosure Management Location
#define AHCI_EM_CTL     0x20    // Enclosure Management Control
#define AHCI_CAP2       0x24    // Host Capabilities Extended
#define AHCI_BOHC       0x28    // BIOS/OS Handoff Control and Status

// Port register offsets (per port, starting at 0x100)
#define AHCI_PxCLB      0x00    // Command List Base Address
#define AHCI_PxCLBU     0x04    // Command List Base Address Upper
#define AHCI_PxFB       0x08    // FIS Base Address
#define AHCI_PxFBU      0x0C    // FIS Base Address Upper
#define AHCI_PxIS       0x10    // Interrupt Status
#define AHCI_PxIE       0x14    // Interrupt Enable
#define AHCI_PxCMD      0x18    // Command and Status
#define AHCI_PxTFD      0x20    // Task File Data
#define AHCI_PxSIG      0x24    // Signature
#define AHCI_PxSSTS     0x28    // Serial ATA Status
#define AHCI_PxSCTL     0x2C    // Serial ATA Control
#define AHCI_PxSERR     0x30    // Serial ATA Error
#define AHCI_PxSACT     0x34    // Serial ATA Active
#define AHCI_PxCI       0x38    // Command Issue
#define AHCI_PxSNTF     0x3C    // Serial ATA Notification
#define AHCI_PxFBS      0x40    // FIS-based Switching Control
#define AHCI_PxDEVSLP   0x44    // Device Sleep

// AHCI capabilities flags
#define AHCI_CAP_S64A   (1 << 31)  // 64-bit addressing
#define AHCI_CAP_SNCQ   (1 << 30)  // Native Command Queuing
#define AHCI_CAP_SSNTF  (1 << 29)  // Staggered Spin-up
#define AHCI_CAP_SMPS   (1 << 28)  // Mechanical Presence Switch
#define AHCI_CAP_SSS    (1 << 27)  // Supports Staggered Spin-up
#define AHCI_CAP_SALP   (1 << 26)  // Aggressive Link Power Management
#define AHCI_CAP_SALST  (1 << 25)  // Aggressive Slumber/Partial
#define AHCI_CAP_SCLO   (1 << 24)  // Command List Override
#define AHCI_CAP_ISS_SHIFT 20      // Interface Speed Support
#define AHCI_CAP_ISS_MASK  (0xF << 20)
#define AHCI_CAP_SNZO   (1 << 19)  // Non-Zero DMA Offsets
#define AHCI_CAP_SAM    (1 << 18)  // AHCI Mode Only
#define AHCI_CAP_SPM    (1 << 17)  // Port Multiplier
#define AHCI_CAP_FBSS   (1 << 16)  // FIS-based Switching
#define AHCI_CAP_PMD    (1 << 15)  // PIO Multiple DRQ Block
#define AHCI_CAP_SSC    (1 << 14)  // Slumber State Capable
#define AHCI_CAP_PSC    (1 << 13)  // Partial State Capable
#define AHCI_CAP_NCS_SHIFT 8       // Number of Command Slots
#define AHCI_CAP_NCS_MASK  (0x1F << 8)
#define AHCI_CAP_CCCS   (1 << 7)   // Command Completion Coalescing
#define AHCI_CAP_EMS    (1 << 6)   // Enclosure Management
#define AHCI_CAP_SXS    (1 << 5)   // Supports External SATA
#define AHCI_CAP_NP_SHIFT 0        // Number of Ports
#define AHCI_CAP_NP_MASK  0x1F

// Port command and status flags
#define AHCI_PxCMD_ST   (1 << 0)   // Start
#define AHCI_PxCMD_SUD  (1 << 1)   // Spin-Up Device
#define AHCI_PxCMD_POD  (1 << 2)   // Power On Device
#define AHCI_PxCMD_CLO  (1 << 3)   // Command List Override
#define AHCI_PxCMD_FRE  (1 << 4)   // FIS Receive Enable
#define AHCI_PxCMD_CCS_SHIFT 8     // Current Command Slot
#define AHCI_PxCMD_CCS_MASK  (0x1F << 8)
#define AHCI_PxCMD_PMA  (1 << 13)  // Port Multiplier Attached
#define AHCI_PxCMD_HPCP (1 << 14)  // Hot Plug Capable Port
#define AHCI_PxCMD_MPSP (1 << 15)  // Mechanical Presence Switch Attached
#define AHCI_PxCMD_CPD  (1 << 16)  // Cold Presence Detection
#define AHCI_PxCMD_ESP  (1 << 21)  // External SATA Port
#define AHCI_PxCMD_FBSCP (1 << 22) // FIS-based Switching Capable Port
#define AHCI_PxCMD_APSTE (1 << 23) // Aggressive Power Management Enable
#define AHCI_PxCMD_ATAPI (1 << 24) // ATAPI
#define AHCI_PxCMD_DLAE (1 << 25)  // Drive LED on ATAPI Enable
#define AHCI_PxCMD_ALPE (1 << 26)  // Aggressive Link Power Management Enable
#define AHCI_PxCMD_ASP  (1 << 27)  // Aggressive Slumber/Partial
#define AHCI_PxCMD_ICC_SHIFT 28    // Interface Communication Control
#define AHCI_PxCMD_ICC_MASK  (0xF << 28)

// FIS types
#define FIS_TYPE_REG_H2D  0x27     // Register FIS - Host to Device
#define FIS_TYPE_REG_D2H  0x34     // Register FIS - Device to Host
#define FIS_TYPE_DMA_ACT  0x39     // DMA Activate FIS
#define FIS_TYPE_DMA_SETUP 0x41    // DMA Setup FIS
#define FIS_TYPE_DATA     0x46     // Data FIS
#define FIS_TYPE_BIST     0x58     // BIST Activate FIS
#define FIS_TYPE_PIO_SETUP 0x5F    // PIO Setup FIS
#define FIS_TYPE_DEV_BITS 0xA1     // Set Device Bits FIS

// Command header flags
#define AHCI_CMD_CFL_SHIFT 0       // Command FIS Length
#define AHCI_CMD_CFL_MASK  0x1F
#define AHCI_CMD_A     (1 << 5)    // ATAPI
#define AHCI_CMD_W     (1 << 6)    // Write
#define AHCI_CMD_P     (1 << 7)    // Prefetchable
#define AHCI_CMD_R     (1 << 8)    // Reset
#define AHCI_CMD_B     (1 << 9)    // BIST
#define AHCI_CMD_C     (1 << 10)   // Clear Busy on R_OK
#define AHCI_CMD_PMP_SHIFT 12      // Port Multiplier Port
#define AHCI_CMD_PMP_MASK  (0xF << 12)
#define AHCI_CMD_PRDTL_SHIFT 16    // Physical Region Descriptor Table Length
#define AHCI_CMD_PRDTL_MASK  (0xFFFF << 16)

// Maximum ports and command slots
#define AHCI_MAX_PORTS 32
#define AHCI_MAX_SLOTS 32

// AHCI device structure
typedef struct {
    uint64_t base_address;         // MMIO base address
    uint32_t num_ports;            // Number of ports
    uint32_t capabilities;         // Host capabilities
    bool present;                  // Is controller present?
    block_device_t block_dev;      // Block device interface
} ahci_controller_t;

// AHCI port structure
typedef struct {
    ahci_controller_t* controller;  // Parent controller
    uint32_t port_num;             // Port number
    bool present;                  // Is device present?
    bool lba48;                    // Supports 48-bit LBA?
    uint64_t sectors;              // Total sectors
    uint32_t sector_size;          // Sector size (usually 512)
    char model[41];                // Drive model
    block_device_t block_dev;      // Block device interface
} ahci_port_t;

// Maximum AHCI devices
#define MAX_AHCI_DEVICES 4

// AHCI functions
error_code_t ahci_init(void);
error_code_t ahci_detect_controllers(void);
ahci_controller_t* ahci_get_controller(uint32_t index);
ahci_port_t* ahci_get_port(uint32_t controller_index, uint32_t port_index);
error_code_t ahci_read_sectors(ahci_port_t* port, uint64_t lba, uint32_t count, void* buffer);
error_code_t ahci_write_sectors(ahci_port_t* port, uint64_t lba, uint32_t count, const void* buffer);

#endif // KERNEL_DRIVERS_AHCI_H

