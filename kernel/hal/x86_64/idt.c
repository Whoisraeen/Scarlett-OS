/**
 * @file idt.c
 * @brief Interrupt Descriptor Table (IDT) setup for x86_64
 * 
 * This file sets up the IDT and exception handlers.
 */

#include "../../include/types.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"

// IDT entry structure
typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

// IDT pointer structure
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

// IDT with 256 entries
static idt_entry_t idt[256];
static idt_ptr_t idt_ptr;

// Type attributes
#define IDT_TYPE_INTERRUPT_GATE 0x8E
#define IDT_TYPE_TRAP_GATE      0x8F

/**
 * Set an IDT entry
 */
static void idt_set_entry(int index, uint64_t offset, uint16_t selector, 
                          uint8_t type_attr, uint8_t ist) {
    idt[index].offset_low = offset & 0xFFFF;
    idt[index].selector = selector;
    idt[index].ist = ist;
    idt[index].type_attr = type_attr;
    idt[index].offset_mid = (offset >> 16) & 0xFFFF;
    idt[index].offset_high = (offset >> 32) & 0xFFFFFFFF;
    idt[index].reserved = 0;
}

// Exception handler stubs (defined in exceptions.S)
extern void exception_handler_0(void);
extern void exception_handler_1(void);
extern void exception_handler_2(void);
extern void exception_handler_3(void);
extern void exception_handler_4(void);
extern void exception_handler_5(void);
extern void exception_handler_6(void);
extern void exception_handler_7(void);
extern void exception_handler_8(void);
extern void exception_handler_9(void);
extern void exception_handler_10(void);
extern void exception_handler_11(void);
extern void exception_handler_12(void);
extern void exception_handler_13(void);
extern void exception_handler_14(void);
extern void exception_handler_15(void);
extern void exception_handler_16(void);
extern void exception_handler_17(void);
extern void exception_handler_18(void);
extern void exception_handler_19(void);
extern void exception_handler_20(void);
extern void exception_handler_21(void);
extern void exception_handler_22(void);
extern void exception_handler_23(void);
extern void exception_handler_24(void);
extern void exception_handler_25(void);
extern void exception_handler_26(void);
extern void exception_handler_27(void);
extern void exception_handler_28(void);
extern void exception_handler_29(void);
extern void exception_handler_30(void);
extern void exception_handler_31(void);

/**
 * Load IDT (defined in assembly)
 */
extern void idt_load(uint64_t idt_ptr_addr);

/**
 * Initialize IDT
 */
void idt_init(void) {
    kinfo("Initializing IDT...\n");
    
    // Clear IDT
    for (int i = 0; i < 256; i++) {
        idt_set_entry(i, 0, 0, 0, 0);
    }
    
    // Set up exception handlers (0-31)
    idt_set_entry(0, (uint64_t)exception_handler_0, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(1, (uint64_t)exception_handler_1, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(2, (uint64_t)exception_handler_2, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(3, (uint64_t)exception_handler_3, 0x08, IDT_TYPE_TRAP_GATE, 0);
    idt_set_entry(4, (uint64_t)exception_handler_4, 0x08, IDT_TYPE_TRAP_GATE, 0);
    idt_set_entry(5, (uint64_t)exception_handler_5, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(6, (uint64_t)exception_handler_6, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(7, (uint64_t)exception_handler_7, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(8, (uint64_t)exception_handler_8, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(9, (uint64_t)exception_handler_9, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(10, (uint64_t)exception_handler_10, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(11, (uint64_t)exception_handler_11, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(12, (uint64_t)exception_handler_12, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(13, (uint64_t)exception_handler_13, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(14, (uint64_t)exception_handler_14, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(15, (uint64_t)exception_handler_15, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(16, (uint64_t)exception_handler_16, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(17, (uint64_t)exception_handler_17, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(18, (uint64_t)exception_handler_18, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(19, (uint64_t)exception_handler_19, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(20, (uint64_t)exception_handler_20, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(21, (uint64_t)exception_handler_21, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(22, (uint64_t)exception_handler_22, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(23, (uint64_t)exception_handler_23, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(24, (uint64_t)exception_handler_24, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(25, (uint64_t)exception_handler_25, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(26, (uint64_t)exception_handler_26, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(27, (uint64_t)exception_handler_27, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(28, (uint64_t)exception_handler_28, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(29, (uint64_t)exception_handler_29, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(30, (uint64_t)exception_handler_30, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(31, (uint64_t)exception_handler_31, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    
    // Set up hardware interrupt handlers (IRQ 0-15 = interrupts 32-47)
    extern void interrupt_handler_32(void);
    extern void interrupt_handler_33(void);
    extern void interrupt_handler_34(void);
    extern void interrupt_handler_35(void);
    extern void interrupt_handler_36(void);
    extern void interrupt_handler_37(void);
    extern void interrupt_handler_38(void);
    extern void interrupt_handler_39(void);
    extern void interrupt_handler_40(void);
    extern void interrupt_handler_41(void);
    extern void interrupt_handler_42(void);
    extern void interrupt_handler_43(void);
    extern void interrupt_handler_44(void);
    extern void interrupt_handler_45(void);
    extern void interrupt_handler_46(void);
    extern void interrupt_handler_47(void);
    
    // Timer interrupt (IRQ 0)
    idt_set_entry(32, (uint64_t)interrupt_handler_32, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    // Keyboard interrupt (IRQ 1)
    idt_set_entry(33, (uint64_t)interrupt_handler_33, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    // Other interrupts (placeholders for now)
    idt_set_entry(34, (uint64_t)interrupt_handler_34, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(35, (uint64_t)interrupt_handler_35, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(36, (uint64_t)interrupt_handler_36, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(37, (uint64_t)interrupt_handler_37, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(38, (uint64_t)interrupt_handler_38, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(39, (uint64_t)interrupt_handler_39, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(40, (uint64_t)interrupt_handler_40, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(41, (uint64_t)interrupt_handler_41, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(42, (uint64_t)interrupt_handler_42, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(43, (uint64_t)interrupt_handler_43, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(44, (uint64_t)interrupt_handler_44, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(45, (uint64_t)interrupt_handler_45, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(46, (uint64_t)interrupt_handler_46, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    idt_set_entry(47, (uint64_t)interrupt_handler_47, 0x08, IDT_TYPE_INTERRUPT_GATE, 0);
    
    // Set up IDT pointer
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t)&idt;
    
    // Load IDT
    idt_load((uint64_t)&idt_ptr);
    
    kinfo("IDT initialized successfully\n");
}

