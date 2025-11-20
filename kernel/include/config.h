/**
 * @file config.h
 * @brief Kernel configuration constants
 * 
 * All magic numbers should be defined here, not scattered throughout code.
 */

#ifndef KERNEL_CONFIG_H
#define KERNEL_CONFIG_H

// ============================================================================
// Memory Configuration
// ============================================================================

// Page sizes
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

// Physical Memory Manager
#define PMM_MAX_PAGES (16 * 1024 * 256)  // Support up to 16GB
#define PMM_BITMAP_SIZE (PMM_MAX_PAGES / 8)

// Kernel memory layout (virtual addresses)
#define KERNEL_VMA_BASE         0x0000000000000000ULL
#define KERNEL_PHYS_BASE        0x0000000000100000ULL  // 1MB
#define PHYS_MAP_BASE           0xFFFF800000000000ULL  // Physical memory direct map

// Heap configuration
#define HEAP_START              0xFFFFFFFFC0000000ULL
#define HEAP_INITIAL_SIZE       (16 * 1024 * 1024)    // 16MB
#define HEAP_MAX_SIZE           (256 * 1024 * 1024)   // 256MB

// Reserved memory
#define RESERVED_LOW_MEMORY     0x0000000000100000ULL  // First 1MB reserved

// ============================================================================
// Serial Port Configuration
// ============================================================================

#define SERIAL_COM1_PORT        0x3F8
#define SERIAL_BAUD_RATE        38400
#define SERIAL_BAUD_DIVISOR     3  // For 38400 baud

// COM port register offsets
#define SERIAL_DATA_REG         0
#define SERIAL_IER_REG          1
#define SERIAL_IIR_REG          2
#define SERIAL_LCR_REG          3
#define SERIAL_MCR_REG          4
#define SERIAL_LSR_REG          5
#define SERIAL_MSR_REG          6
#define SERIAL_SCRATCH_REG      7

// Line status register bits
#define SERIAL_LSR_DATA_READY       (1 << 0)
#define SERIAL_LSR_OVERRUN_ERROR    (1 << 1)
#define SERIAL_LSR_PARITY_ERROR     (1 << 2)
#define SERIAL_LSR_FRAMING_ERROR    (1 << 3)
#define SERIAL_LSR_BREAK_INT        (1 << 4)
#define SERIAL_LSR_THR_EMPTY        (1 << 5)
#define SERIAL_LSR_TRANSMITTER_EMPTY (1 << 6)

// ============================================================================
// GDT Configuration
// ============================================================================

#define GDT_ENTRY_COUNT         5

// GDT access flags
#define GDT_ACCESS_PRESENT      (1 << 7)
#define GDT_ACCESS_RING0        (0 << 5)
#define GDT_ACCESS_RING3        (3 << 5)
#define GDT_ACCESS_SYSTEM       (1 << 4)
#define GDT_ACCESS_EXECUTABLE   (1 << 3)
#define GDT_ACCESS_DC           (1 << 2)
#define GDT_ACCESS_RW           (1 << 1)
#define GDT_ACCESS_ACCESSED     (1 << 0)

// GDT granularity flags
#define GDT_GRAN_64BIT          (1 << 5)
#define GDT_GRAN_32BIT          (1 << 6)
#define GDT_GRAN_4K_BLOCKS      (1 << 7)

// ============================================================================
// IDT Configuration
// ============================================================================

#define IDT_ENTRY_COUNT         256
#define IDT_EXCEPTION_COUNT     32

// IDT gate types
#define IDT_TYPE_INTERRUPT_GATE 0x8E
#define IDT_TYPE_TRAP_GATE      0x8F

// ============================================================================
// Boot Information
// ============================================================================

// BOOT_INFO_MAGIC defined in bootloader/common/boot_info.h
// #define BOOT_INFO_MAGIC         0x534341524C545400ULL  // "SCARLTT\0"
#define MAX_MEMORY_REGIONS      128
#define MAX_CMDLINE_LENGTH      256

// ============================================================================
// Buffer Sizes
// ============================================================================

#define KPRINTF_BUFFER_SIZE     256
#define ITOA_BUFFER_SIZE        32
#define THREAD_NAME_MAX         32
#define KERNEL_STACK_SIZE       (64 * 1024)  // 64KB

// ============================================================================
// Version Information
// ============================================================================

#define KERNEL_VERSION_MAJOR    0
#define KERNEL_VERSION_MINOR    1
#define KERNEL_VERSION_PATCH    0
#define KERNEL_VERSION_STRING   "0.1.0"
#define KERNEL_NAME             "Scarlett OS"

#endif // KERNEL_CONFIG_H

