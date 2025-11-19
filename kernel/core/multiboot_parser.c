#include "../include/multiboot2.h"
#include "../../bootloader/common/boot_info.h"
#include "../include/kprintf.h"
#include "../include/string.h"

void parse_multiboot_info(boot_info_t* boot_info, void* multiboot_info_ptr) {
    if (!boot_info || !multiboot_info_ptr) {
        return;
    }

    memset(boot_info, 0, sizeof(boot_info_t));
    boot_info->magic = BOOT_INFO_MAGIC;

    multiboot_tag_t* tag = (multiboot_tag_t*)(multiboot_info_ptr + 8);

    while (tag->type != MULTIBOOT_TAG_TYPE_END) {
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
                if (fb_tag->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
                    boot_info->framebuffer.base = fb_tag->framebuffer_addr;
                    boot_info->framebuffer.width = fb_tag->framebuffer_width;
                    boot_info->framebuffer.height = fb_tag->framebuffer_height;
                    boot_info->framebuffer.pitch = fb_tag->framebuffer_pitch;
                    boot_info->framebuffer.bpp = fb_tag->framebuffer_bpp;
                }
                break;
            }
        }
        tag = (multiboot_tag_t*)((uint8_t*)tag + ((tag->size + 7) & ~7));
    }
}
