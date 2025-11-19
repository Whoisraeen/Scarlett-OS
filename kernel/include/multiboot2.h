#ifndef _MULTIBOOT2_H
#define _MULTIBOOT2_H

#include "types.h"

/* Multiboot2 header */
#define MULTIBOOT2_HEADER_MAGIC         0xE85250D6

/* Magic number passed to kernel in EAX */
#define MULTIBOOT2_BOOTLOADER_MAGIC     0x36D76289

/* Tag types */
#define MULTIBOOT_TAG_TYPE_END                  0
#define MULTIBOOT_TAG_TYPE_CMDLINE              1
#define MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME     2
#define MULTIBOOT_TAG_TYPE_MODULE               3
#define MULTIBOOT_TAG_TYPE_BASIC_MEMINFO        4
#define MULTIBOOT_TAG_TYPE_BOOTDEV              5
#define MULTIBOOT_TAG_TYPE_MMAP                 6
#define MULTIBOOT_TAG_TYPE_VBE                  7
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER          8
#define MULTIBOOT_TAG_TYPE_ELF_SECTIONS         9
#define MULTIBOOT_TAG_TYPE_APM                  10
#define MULTIBOOT_TAG_TYPE_EFI32_IH             11
#define MULTIBOOT_TAG_TYPE_EFI64_IH             12
#define MULTIBOOT_TAG_TYPE_SMBIOS               13
#define MULTIBOOT_TAG_TYPE_ACPI_OLD             14
#define MULTIBOOT_TAG_TYPE_ACPI_NEW             15
#define MULTIBOOT_TAG_TYPE_NETWORK              16
#define MULTIBOOT_TAG_TYPE_EFI_MMAP             17
#define MULTIBOOT_TAG_TYPE_EFI_BS               18
#define MULTIBOOT_TAG_TYPE_EFI32_IHP            19
#define MULTIBOOT_TAG_TYPE_EFI64_IHP            20
#define MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR       21

/* Base tag structure */
typedef struct {
    uint32_t type;
    uint32_t size;
} __attribute__((packed)) multiboot_tag_t;

/* Memory map tag structure */
typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    // Followed by a variable number of memory map entries
} __attribute__((packed)) multiboot_tag_mmap_t;

typedef struct {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed)) multiboot_mmap_entry_t;

#define MULTIBOOT_MEMORY_AVAILABLE              1
#define MULTIBOOT_MEMORY_RESERVED               2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE       3
#define MULTIBOOT_MEMORY_NVS                    4
#define MULTIBOOT_MEMORY_BADRAM                 5

/* Framebuffer tag structure */
typedef struct {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t reserved;
    // Color info (variable size) follows
} __attribute__((packed)) multiboot_tag_framebuffer_t;

#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED      0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB          1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT     2

#endif /* _MULTIBOOT2_H */
