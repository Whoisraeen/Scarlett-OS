/**
 * @file gdt.c
 * @brief Global Descriptor Table (GDT) setup for x86_64
 * 
 * In x86_64 long mode, segmentation is largely unused, but we still
 * need a valid GDT with appropriate code and data segments.
 */

#include "../../include/types.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"

// GDT entry structure
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

// GDT pointer structure
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

// GDT entries
static gdt_entry_t gdt[5];
static gdt_ptr_t gdt_ptr;

// Access byte flags
#define GDT_ACCESS_PRESENT    (1 << 7)
#define GDT_ACCESS_RING0      (0 << 5)
#define GDT_ACCESS_RING3      (3 << 5)
#define GDT_ACCESS_SYSTEM     (1 << 4)
#define GDT_ACCESS_EXECUTABLE (1 << 3)
#define GDT_ACCESS_DC         (1 << 2)  // Direction/Conforming
#define GDT_ACCESS_RW         (1 << 1)  // Read/Write
#define GDT_ACCESS_ACCESSED   (1 << 0)

// Granularity byte flags
#define GDT_GRAN_64BIT        (1 << 5)
#define GDT_GRAN_32BIT        (1 << 6)
#define GDT_GRAN_4K_BLOCKS    (1 << 7)

/**
 * Set a GDT entry
 */
static void gdt_set_entry(int index, uint32_t base, uint32_t limit, 
                          uint8_t access, uint8_t gran) {
    gdt[index].limit_low = limit & 0xFFFF;
    gdt[index].base_low = base & 0xFFFF;
    gdt[index].base_middle = (base >> 16) & 0xFF;
    gdt[index].access = access;
    gdt[index].granularity = (limit >> 16) & 0x0F;
    gdt[index].granularity |= gran & 0xF0;
    gdt[index].base_high = (base >> 24) & 0xFF;
}

/**
 * Load GDT (defined in assembly)
 */
extern void gdt_load(uint64_t gdt_ptr_addr);

/**
 * Initialize GDT
 */
void gdt_init(void) {
    kinfo("Initializing GDT...\n");
    
    // Entry 0: Null descriptor
    gdt_set_entry(0, 0, 0, 0, 0);
    
    // Entry 1: Kernel code segment (64-bit)
    gdt_set_entry(1, 0, 0xFFFFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | 
                  GDT_ACCESS_SYSTEM | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
                  GDT_GRAN_64BIT | GDT_GRAN_4K_BLOCKS);
    
    // Entry 2: Kernel data segment
    gdt_set_entry(2, 0, 0xFFFFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | 
                  GDT_ACCESS_SYSTEM | GDT_ACCESS_RW,
                  GDT_GRAN_4K_BLOCKS);
    
    // Entry 3: User code segment (64-bit)
    gdt_set_entry(3, 0, 0xFFFFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | 
                  GDT_ACCESS_SYSTEM | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
                  GDT_GRAN_64BIT | GDT_GRAN_4K_BLOCKS);
    
    // Entry 4: User data segment
    gdt_set_entry(4, 0, 0xFFFFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | 
                  GDT_ACCESS_SYSTEM | GDT_ACCESS_RW,
                  GDT_GRAN_4K_BLOCKS);
    
    // Set up GDT pointer
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base = (uint64_t)&gdt;
    
    // Load GDT
    gdt_load((uint64_t)&gdt_ptr);
    
    kinfo("GDT initialized successfully\n");
}

