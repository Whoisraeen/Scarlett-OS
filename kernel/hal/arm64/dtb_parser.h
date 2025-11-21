/**
 * @file dtb_parser.h
 * @brief Device Tree Blob Parser for ARM64
 */

#ifndef DTB_PARSER_H
#define DTB_PARSER_H

#include <stdint.h>

// DTB header structure
typedef struct {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
} dtb_header_t;

// Device tree node
typedef struct dtb_node {
    char name[64];
    uint32_t phandle;
    struct dtb_node* parent;
    struct dtb_node* child;
    struct dtb_node* sibling;
} dtb_node_t;

// Device tree property
typedef struct {
    char name[64];
    void* data;
    uint32_t length;
} dtb_property_t;

// DTB parser functions
int dtb_parse(void* dtb_addr);
dtb_node_t* dtb_find_node(const char* path);
dtb_property_t* dtb_get_property(dtb_node_t* node, const char* name);
uint32_t dtb_get_u32(dtb_property_t* prop, int index);
uint64_t dtb_get_u64(dtb_property_t* prop, int index);
const char* dtb_get_string(dtb_property_t* prop);

// Device enumeration
int dtb_enumerate_devices(void);
void dtb_print_tree(void);

#endif // DTB_PARSER_H
