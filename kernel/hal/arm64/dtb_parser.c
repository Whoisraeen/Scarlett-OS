/**
 * @file dtb_parser.c
 * @brief Device Tree Blob Parser Implementation
 */

#include "dtb_parser.h"
#include <stdio.h>
#include <string.h>
#include "../../include/mm/heap.h"

#define DTB_MAGIC 0xD00DFEED
#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE 0x00000002
#define FDT_PROP 0x00000003
#define FDT_NOP 0x00000004
#define FDT_END 0x00000009

static dtb_header_t* g_dtb_header = NULL;
static dtb_node_t* g_root_node = NULL;
static char* g_strings_block = NULL;
static uint32_t* g_struct_block = NULL;

// Byte swap for big-endian DTB
static uint32_t be32_to_cpu(uint32_t val) {
    return ((val & 0xFF000000) >> 24) |
           ((val & 0x00FF0000) >> 8) |
           ((val & 0x0000FF00) << 8) |
           ((val & 0x000000FF) << 24);
}

static uint64_t be64_to_cpu(uint64_t val) {
    return ((uint64_t)be32_to_cpu(val & 0xFFFFFFFF) << 32) |
           be32_to_cpu(val >> 32);
}

// Helper: Align pointer to 4 bytes
static uint32_t* align_4(uint32_t* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    return (uint32_t*)((addr + 3) & ~3);
}

// Helper: Parse a single node
// Returns pointer to next token after this node's subtree
static uint32_t* dtb_parse_node(uint32_t* token_ptr, dtb_node_t* parent, dtb_node_t** result) {
    if (be32_to_cpu(*token_ptr) != FDT_BEGIN_NODE) {
        return token_ptr;
    }
    token_ptr++;
    
    // Parse Name
    char* name = (char*)token_ptr;
    size_t name_len = strlen(name);
    size_t len_aligned = (name_len + 1 + 3) & ~3;
    token_ptr = (uint32_t*)((char*)token_ptr + len_aligned);
    
    // Create Node
    dtb_node_t* node = (dtb_node_t*)kmalloc(sizeof(dtb_node_t));
    memset(node, 0, sizeof(dtb_node_t));
    strncpy(node->name, name, 63);
    node->parent = parent;
    
    // Parse Properties and Children
    dtb_node_t* last_child = NULL;
    dtb_property_t* last_prop = NULL;
    
    while (1) {
        uint32_t token = be32_to_cpu(*token_ptr);
        
        if (token == FDT_NOP) {
            token_ptr++;
            continue;
        }
        else if (token == FDT_PROP) {
            token_ptr++;
            uint32_t len = be32_to_cpu(*token_ptr++);
            uint32_t nameoff = be32_to_cpu(*token_ptr++);
            
            dtb_property_t* prop = (dtb_property_t*)kmalloc(sizeof(dtb_property_t));
            memset(prop, 0, sizeof(dtb_property_t));
            
            // Get property name from strings block
            const char* prop_name = g_strings_block + nameoff;
            strncpy(prop->name, prop_name, 63);
            
            prop->length = len;
            if (len > 0) {
                prop->data = kmalloc(len);
                memcpy(prop->data, token_ptr, len);
                
                // Advance token ptr past data, aligned to 4 bytes
                size_t data_len_aligned = (len + 3) & ~3;
                token_ptr = (uint32_t*)((char*)token_ptr + data_len_aligned);
            }
            
            // Link property
            if (last_prop) {
                last_prop->next = prop;
            } else {
                node->properties = prop;
            }
            last_prop = prop;
        }
        else if (token == FDT_BEGIN_NODE) {
            dtb_node_t* child_node = NULL;
            token_ptr = dtb_parse_node(token_ptr, node, &child_node);
            
            if (child_node) {
                if (last_child) {
                    last_child->sibling = child_node;
                } else {
                    node->child = child_node;
                }
                last_child = child_node;
            }
        }
        else if (token == FDT_END_NODE) {
            token_ptr++;
            break;
        }
        else if (token == FDT_END) {
            break;
        }
        else {
            printf("DTB: Unknown token %08x\n", token);
            token_ptr++;
        }
    }
    
    *result = node;
    return token_ptr;
}

int dtb_parse(void* dtb_addr) {
    g_dtb_header = (dtb_header_t*)dtb_addr;
    
    // Verify magic number
    if (be32_to_cpu(g_dtb_header->magic) != DTB_MAGIC) {
        printf("DTB: Invalid magic number\n");
        return -1;
    }
    
    printf("DTB: Found valid device tree\n");
    printf("DTB: Version %u, size %u bytes\n",
           be32_to_cpu(g_dtb_header->version),
           be32_to_cpu(g_dtb_header->totalsize));
    
    // Get strings block
    g_strings_block = (char*)dtb_addr + be32_to_cpu(g_dtb_header->off_dt_strings);
    
    // Get struct block
    g_struct_block = (uint32_t*)((char*)dtb_addr + be32_to_cpu(g_dtb_header->off_dt_struct));
    
    // Parse entire tree recursively
    uint32_t* token_ptr = g_struct_block;
    dtb_parse_node(token_ptr, NULL, &g_root_node);
    
    return 0;
}

dtb_node_t* dtb_get_root_node(void) {
    return g_root_node;
}

dt b_node_t* dtb_find_node(const char* path) {
    if (!g_root_node || !path) {
        return NULL;
    }
    
    if (strcmp(path, "/") == 0) {
        return g_root_node;
    }
    
    // Path parsing logic: /path/to/node
    // TODO: Implement full path traversal - DONE: Full path traversal implemented
    // This function now properly traverses the full path by:
    // 1. Splitting path into components
    // 2. For each component, searching all children (including siblings)
    // 3. Handling node names with @address suffixes
    // 4. Recursively descending through the tree structure
    
    char path_buf[256];
    strncpy(path_buf, path, 255);
    path_buf[255] = '\0';  // Ensure null termination
    
    dtb_node_t* current = g_root_node;
    char* component = strtok(path_buf, "/");
    
    while (component) {
        // Search children of current (including all siblings in the child list)
        dtb_node_t* child = current->child;
        bool found = false;
        
        while (child) {
            // Node names in DTB are often "name@address". 
            // Search for exact match or match before '@'
            size_t cmp_len = strlen(component);
            if (strncmp(child->name, component, cmp_len) == 0) {
                if (child->name[cmp_len] == '\0' || child->name[cmp_len] == '@') {
                    current = child;
                    found = true;
                    break;
                }
            }
            child = child->sibling;
        }
        
        if (!found) {
            // Path component not found
            return NULL;
        }
        
        component = strtok(NULL, "/");
    }
    
    return current;
}

dt b_property_t* dtb_get_property(dtb_node_t* node, const char* name) {
    if (!node || !name) return NULL;
    
    dtb_property_t* prop = node->properties;
    while (prop) {
        if (strcmp(prop->name, name) == 0) {
            return prop;
        }
        prop = prop->next;
    }
    
    return NULL;
}

uint32_t dtb_get_u32(dtb_property_t* prop, int index) {
    if (!prop || !prop->data || index * 4 >= prop->length) {
        return 0;
    }
    
    uint32_t* data = (uint32_t*)prop->data;
    return be32_to_cpu(data[index]);
}

uint64_t dtb_get_u64(dtb_property_t* prop, int index) {
    if (!prop || !prop->data || index * 8 >= prop->length) {
        return 0;
    }
    
    uint64_t* data = (uint64_t*)prop->data;
    return be64_to_cpu(data[index]);
}

const char* dtb_get_string(dtb_property_t* prop) {
    if (!prop || !prop->data) {
        return "";
    }
    
    return (const char*)prop->data;
}

// Helper for enumeration
static void dtb_walk_tree(dtb_node_t* node, void (*callback)(dtb_node_t*)) {
    if (!node) return;
    
    callback(node);
    
    dtb_walk_tree(node->child, callback);
    dtb_walk_tree(node->sibling, callback);
}

static void enumerate_device_callback(dtb_node_t* node) {
    dtb_property_t* compat = dtb_get_property(node, "compatible");
    if (compat) {
        // This is a device node with compatible string
        printf("DTB: Found device '%s', compatible: '%s'\n", 
               node->name, dtb_get_string(compat));
               
        // Extract reg (memory regions)
        dtb_property_t* reg = dtb_get_property(node, "reg");
        if (reg) {
            // This is simplified; dealing with #address-cells and #size-cells from parent is needed for full correctness
            // but getting the raw values is a good step.
            // Assuming 64-bit address and size for now (ARM64 typical)
            // uint64_t addr = dtb_get_u64(reg, 0);
            // uint64_t size = dtb_get_u64(reg, 1);
            // printf("     Reg: 0x%lx (Size: 0x%lx)\n", addr, size);
        }
        
        // Extract interrupts
        dtb_property_t* irq = dtb_get_property(node, "interrupts");
        if (irq) {
             // printf("     Has interrupts\n");
        }
    }
}

int dtb_enumerate_devices(void) {
    if (!g_root_node) {
        printf("DTB: No device tree loaded\n");
        return -1;
    }
    
    printf("DTB: Enumerating devices...\n");
    // TODO: Walk device tree and enumerate devices - DONE: Tree walking implemented
    // The dtb_walk_tree function recursively walks the entire device tree,
    // calling enumerate_device_callback for each node to identify devices
    dtb_walk_tree(g_root_node, enumerate_device_callback);
    printf("DTB: Device enumeration complete\n");
    return 0;
}

static void print_node_recursive(dtb_node_t* node, int depth) {
    if (!node) return;
    
    for (int i = 0; i < depth; i++) printf("  ");
    printf("%s\n", node->name);
    
    dtb_property_t* prop = node->properties;
    while (prop) {
        for (int i = 0; i < depth + 1; i++) printf("  ");
        printf("- %s (len %d)\n", prop->name, prop->length);
        prop = prop->next;
    }
    
    print_node_recursive(node->child, depth + 1);
    print_node_recursive(node->sibling, depth); // Sibling is same depth, but wait, logic above calls sibling
    // Logic issue in walk: recursion usually handles children, loops handle siblings.
    // My recursion logic:
    // print(node) -> print(child) -> print(child's sibling) ...
}

// Recursively print tree structure
static void visit_node_print(dtb_node_t* node, int depth) {
    if (!node) return;
    
    // Print indentation based on depth
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
    
    // Print node name
    printf("%s\n", node->name);
    
    // Print properties
    dtb_property_t* prop = node->properties;
    while (prop) {
        for (int i = 0; i < depth + 1; i++) {
            printf("  ");
        }
        printf("- %s", prop->name);
        if (prop->length > 0) {
            printf(" (len %d)", prop->length);
        }
        printf("\n");
        prop = prop->next;
    }
    
    // Recursively print children (depth increases)
    visit_node_print(node->child, depth + 1);
    
    // Print siblings at same depth
    visit_node_print(node->sibling, depth);
}

void dtb_print_tree(void) {
    if (!g_root_node) {
        printf("DTB: No device tree loaded\n");
        return;
    }
    
    printf("Device Tree:\n");
    // TODO: Recursively print tree structure - DONE: Recursive printing implemented
    // The visit_node_print function recursively traverses the tree,
    // printing nodes with proper indentation based on depth
    visit_node_print(g_root_node, 0);
}