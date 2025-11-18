/**
 * @file boot_info.h
 * @brief Boot information structure shared between bootloader and kernel
 * 
 * This header defines the structure used to pass information from the
 * bootloader to the kernel during boot.
 */

#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include <stdint.h>

#define BOOT_INFO_MAGIC 0x534341524C545400  // "SCARLTT\0"
#define MAX_MEMORY_REGIONS 128
#define MAX_CMDLINE_LENGTH 256

/**
 * Memory region types (from UEFI)
 */
typedef enum {
    MEMORY_TYPE_RESERVED = 0,
    MEMORY_TYPE_LOADER_CODE = 1,
    MEMORY_TYPE_LOADER_DATA = 2,
    MEMORY_TYPE_BOOT_SERVICES_CODE = 3,
    MEMORY_TYPE_BOOT_SERVICES_DATA = 4,
    MEMORY_TYPE_RUNTIME_SERVICES_CODE = 5,
    MEMORY_TYPE_RUNTIME_SERVICES_DATA = 6,
    MEMORY_TYPE_CONVENTIONAL = 7,
    MEMORY_TYPE_UNUSABLE = 8,
    MEMORY_TYPE_ACPI_RECLAIM = 9,
    MEMORY_TYPE_ACPI_NVS = 10,
    MEMORY_TYPE_MMIO = 11,
    MEMORY_TYPE_MMIO_PORT_SPACE = 12,
    MEMORY_TYPE_PAL_CODE = 13,
    MEMORY_TYPE_PERSISTENT = 14
} memory_type_t;

/**
 * Memory region descriptor
 */
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t padding;
} memory_region_t;

/**
 * Framebuffer information
 */
typedef struct {
    uint64_t base;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t reserved_mask;
} framebuffer_info_t;

/**
 * Boot information structure passed to kernel
 */
typedef struct {
    // Magic number for verification
    uint64_t magic;
    
    // Memory map
    uint32_t memory_map_count;
    uint32_t padding1;
    memory_region_t memory_map[MAX_MEMORY_REGIONS];
    
    // Framebuffer information
    framebuffer_info_t framebuffer;
    
    // ACPI tables
    uint64_t rsdp_address;
    
    // Kernel information
    uint64_t kernel_physical_base;
    uint64_t kernel_virtual_base;
    uint64_t kernel_size;
    
    // Command line
    char cmdline[MAX_CMDLINE_LENGTH];
    
    // Bootloader information
    char bootloader_name[64];
    uint32_t bootloader_version;
    
    // Reserved for future use
    uint8_t reserved[256];
} boot_info_t;

#endif // BOOT_INFO_H

