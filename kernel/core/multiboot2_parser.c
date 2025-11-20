#include "../include/types.h"
#include "../include/boot/multiboot2.h"
#include "../../bootloader/common/boot_info.h"
#include "../include/kprintf.h"

// Helper to align to 8 bytes
#define ALIGN_8(x) ((x + 7) & ~7)

void multiboot2_parse(uint64_t addr, boot_info_t* boot_info) {
    struct multiboot_tag *tag;
    
    // Clear boot info first
    // memset(boot_info, 0, sizeof(boot_info_t)); // Assuming memset exists or manual clear
    for (uint32_t i = 0; i < sizeof(boot_info_t); i++) {
        ((uint8_t*)boot_info)[i] = 0;
    }

    boot_info->magic = BOOT_INFO_MAGIC;
    
    // The first 8 bytes are total size and reserved
    uint32_t total_size = *(uint32_t*)addr;
    
    // Skip the fixed part
    tag = (struct multiboot_tag *)(addr + 8);

    kinfo("Parsing Multiboot2 info at 0x%016lx (size: %u)\n", addr, total_size);

    while (tag->type != MULTIBOOT_TAG_TYPE_END) {
        // kinfo("Tag type: %u, size: %u\n", tag->type, tag->size);
        
        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME: {
                struct multiboot_tag_string *tag_name = (struct multiboot_tag_string *)tag;
                // Simple copy, careful with buffer overflow in real code
                char* src = tag_name->string;
                char* dst = boot_info->bootloader_name;
                int i = 0;
                while (*src && i < 63) {
                    dst[i++] = *src++;
                }
                dst[i] = 0;
                boot_info->bootloader_version = 0x00020000; // Fake version 2.0
                break;
            }
            
            case MULTIBOOT_TAG_TYPE_MMAP: {
                struct multiboot_tag_mmap *tag_mmap = (struct multiboot_tag_mmap *)tag;
                struct multiboot_mmap_entry *entry = tag_mmap->entries;
                
                uint32_t num_entries = (tag_mmap->size - sizeof(struct multiboot_tag_mmap)) / tag_mmap->entry_size;
                
                boot_info->memory_map_count = 0;
                
                for (uint32_t i = 0; i < num_entries && i < MAX_MEMORY_REGIONS; i++) {
                    boot_info->memory_map[i].base = entry->addr;
                    boot_info->memory_map[i].length = entry->len;
                    
                    // Map Multiboot2 types to our types
                    switch (entry->type) {
                        case 1: boot_info->memory_map[i].type = MEMORY_TYPE_CONVENTIONAL; break;
                        case 3: boot_info->memory_map[i].type = MEMORY_TYPE_ACPI_RECLAIM; break;
                        case 4: boot_info->memory_map[i].type = MEMORY_TYPE_ACPI_NVS; break;
                        default: boot_info->memory_map[i].type = MEMORY_TYPE_RESERVED; break;
                    }
                    
                    boot_info->memory_map_count++;
                    entry = (struct multiboot_mmap_entry *)((uint64_t)entry + tag_mmap->entry_size);
                }
                break;
            }
            
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: {
                struct multiboot_tag_framebuffer *tag_fb = (struct multiboot_tag_framebuffer *)tag;
                
                boot_info->framebuffer.base = tag_fb->common_address;
                boot_info->framebuffer.width = tag_fb->common_width;
                boot_info->framebuffer.height = tag_fb->common_height;
                boot_info->framebuffer.pitch = tag_fb->common_pitch;
                boot_info->framebuffer.bpp = tag_fb->common_bpp;
                
                // Default masks for 32bpp (BGRA usually)
                // boot_info has masks (uint32_t), not size/shift
                // Assuming standard 8-8-8-8 layout
                boot_info->framebuffer.red_mask = 0x00FF0000;
                boot_info->framebuffer.green_mask = 0x0000FF00;
                boot_info->framebuffer.blue_mask = 0x000000FF;
                boot_info->framebuffer.reserved_mask = 0xFF000000;
                break;
            }
        }
        
        // Move to next tag, aligned to 8 bytes
        tag = (struct multiboot_tag *)((uint8_t *)tag + ALIGN_8(tag->size));
        
        // Safety check to prevent infinite loop if size is 0 (shouldn't happen)
        if (tag->size == 0) break;
        
        // Safety check for bounds
        if ((uint64_t)tag >= addr + total_size) break;
    }
}
