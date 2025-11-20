#include "../include/multiboot2.h"
#include "../../bootloader/common/boot_info.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"

void parse_multiboot_info(boot_info_t* boot_info, void* multiboot_info_ptr) {
    if (!boot_info || !multiboot_info_ptr) {
        return;
    }

    memset(boot_info, 0, sizeof(boot_info_t));
    boot_info->magic = BOOT_INFO_MAGIC;

    multiboot_tag_t* tag = (multiboot_tag_t*)(multiboot_info_ptr + 8);

    while (tag->type != MULTIBOOT_TAG_TYPE_END) {
        // kinfo("Multiboot tag: %u, size: %u\n", tag->type, tag->size);
        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_MMAP: {
                multiboot_tag_mmap_t* mmap_tag = (multiboot_tag_mmap_t*)tag;
                if (mmap_tag->entry_size == 0) break;

                uint32_t entry_count = (mmap_tag->size - sizeof(multiboot_tag_mmap_t)) / mmap_tag->entry_size;
                if (entry_count > MAX_MEMORY_REGIONS) {
                    entry_count = MAX_MEMORY_REGIONS;
                }
                boot_info->memory_map_count = entry_count;

                multiboot_mmap_entry_t* entry = (multiboot_mmap_entry_t*)((uint8_t*)mmap_tag + sizeof(multiboot_tag_mmap_t));
                for (uint32_t i = 0; i < entry_count; i++) {
                    boot_info->memory_map[i].base = entry->base_addr;
                    boot_info->memory_map[i].length = entry->length;
                    boot_info->memory_map[i].type = (entry->type == MULTIBOOT_MEMORY_AVAILABLE) ? MEMORY_TYPE_CONVENTIONAL : MEMORY_TYPE_RESERVED;
                    
                    entry = (multiboot_mmap_entry_t*)(((uint8_t*)entry) + mmap_tag->entry_size);
                }
                break;
            }
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: {
                multiboot_tag_framebuffer_t* fb_tag = (multiboot_tag_framebuffer_t*)tag;
                kinfo("Found Framebuffer tag: type=%u, addr=0x%016lx, %ux%u @ %u\n", 
                      fb_tag->framebuffer_type, fb_tag->framebuffer_addr,
                      fb_tag->framebuffer_width, fb_tag->framebuffer_height,
                      fb_tag->framebuffer_bpp);
                      
                if (fb_tag->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
                    boot_info->framebuffer.base = fb_tag->framebuffer_addr;
                    boot_info->framebuffer.width = fb_tag->framebuffer_width;
                    boot_info->framebuffer.height = fb_tag->framebuffer_height;
                    boot_info->framebuffer.pitch = fb_tag->framebuffer_pitch;
                    boot_info->framebuffer.bpp = fb_tag->framebuffer_bpp;
                    
                    // Color masks
                    boot_info->framebuffer.red_mask = (fb_tag->framebuffer_type == 1) ? 16 : 0; // Assuming standard 32-bit
                    boot_info->framebuffer.green_mask = (fb_tag->framebuffer_type == 1) ? 8 : 0;
                    boot_info->framebuffer.blue_mask = (fb_tag->framebuffer_type == 1) ? 0 : 0;
                } else {
                    kwarn("Framebuffer type %u not supported (only RGB=1 supported)\n", fb_tag->framebuffer_type);
                }
                break;
            }
            default:
                // kinfo("Ignored tag type %u\n", tag->type);
                break;
        }
        tag = (multiboot_tag_t*)((uint8_t*)tag + ((tag->size + 7) & ~7));
    }
}
