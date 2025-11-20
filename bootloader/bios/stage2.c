/**
 * @file stage2.c
 * @brief BIOS bootloader second stage
 * 
 * Loads kernel from disk and jumps to it
 */

#include <stdint.h>
#include <stdbool.h>
#include "../common/boot_info.h"

// BIOS interrupt functions
static inline void bios_int(uint8_t int_num, uint8_t ah, uint8_t al,
                            uint16_t cx, uint16_t dx, uint16_t bx) {
    __asm__ volatile(
        "int $0x13"
        :
        : "a"((uint16_t)(ah << 8 | al)),
          "c"(cx),
          "d"(dx),
          "b"(bx)
        : "memory"
    );
}

/**
 * Read sectors from disk
 */
static bool read_sectors(uint8_t drive, uint8_t num_sectors,
                         uint16_t cylinder, uint8_t head, uint8_t sector,
                         void* buffer) {
    uint8_t ah = 0x02;  // Read sectors
    uint8_t al = num_sectors;
    uint16_t cx = (cylinder & 0xFF) << 8 | ((cylinder >> 2) & 0xC0) | (sector & 0x3F);
    uint16_t dx = (head & 0xFF) << 8 | drive;
    
    // TODO: Implement actual disk read using BIOS int 0x13
    // For now, this is a placeholder
    
    return true;
}

/**
 * Detect available memory
 */
static void detect_memory(boot_info_t* boot_info) {
    // Use BIOS int 0x15, function 0xE820 to get memory map
    // This is a simplified version - full implementation would iterate
    // through all memory regions
    
    boot_info->memory_map_count = 0;
    // TODO: Implement E820 memory detection
}

/**
 * Main second stage entry point
 */
void stage2_main(uint8_t boot_drive) {
    boot_info_t boot_info = {0};
    
    // Detect memory
    detect_memory(&boot_info);
    
    // Load kernel from disk
    // TODO: Load kernel.elf from disk
    // For now, assume kernel is loaded at a fixed address
    
    // Set up basic environment
    // TODO: Set up GDT, enable A20 line, enter protected mode
    
    // Jump to kernel
    // TODO: Parse ELF and jump to kernel entry point
}

