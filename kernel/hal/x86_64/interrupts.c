/**
 * @file interrupts.c
 * @brief Hardware interrupt handling
 */

#include "../../include/types.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"

// Interrupt handler structure
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t interrupt_num;
    uint64_t error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) interrupt_frame_t;

// External interrupt handler stubs
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

/**
 * Write byte to I/O port
 */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/**
 * Send EOI (End of Interrupt) to PIC
 */
static inline void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(0xA0, 0x20);  // Slave PIC EOI
    }
    outb(0x20, 0x20);  // Master PIC EOI
}

// Direct VGA debugging (write to VGA memory at 0xB8000)
static void vga_putchar_at(int x, int y, char c, uint8_t color) {
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    vga[y * 80 + x] = (color << 8) | c;
}

/**
 * Common interrupt handler
 */
void interrupt_handler_c(interrupt_frame_t* frame) {
    static int irq_count = 0;
    uint64_t interrupt_num = frame->interrupt_num;
    uint8_t irq = (uint8_t)(interrupt_num - 32);

    // Handle timer interrupt (IRQ 0 = interrupt 32)
    if (interrupt_num == 32) {
        // Debug: Show that we're handling interrupts (top-right corner)
        if (irq_count < 10) {
            vga_putchar_at(79, 0, '0' + irq_count, 0x0F);
            irq_count++;
        }

        extern void timer_interrupt_handler(void);
        timer_interrupt_handler();

        // Send EOI (End of Interrupt) to PIC
        pic_send_eoi(irq);

        // Check if we need to reschedule (preemptive multitasking)
        extern void scheduler_check_reschedule(void);
        scheduler_check_reschedule();

        return;
    }
    
    // Handle keyboard interrupt (IRQ 1 = interrupt 33)
    if (interrupt_num == 33) {
        extern void keyboard_interrupt_handler(void);
        keyboard_interrupt_handler();
        
        // Send EOI to PIC
        pic_send_eoi(irq);
        return;
    }
    
    // Handle mouse interrupt (IRQ 12 = interrupt 44)
    if (interrupt_num == 44) {
        extern void mouse_interrupt_handler(void);
        mouse_interrupt_handler();
        
        // Send EOI to PIC
        pic_send_eoi(irq);
        return;
    }
    
    // Handle other interrupts
    kdebug("Unhandled interrupt: %lu (IRQ %u)\n", interrupt_num, irq);
    
    // Send EOI for all interrupts
    pic_send_eoi(irq);
}

/**
 * Initialize PIC (Programmable Interrupt Controller)
 */
void pic_init(void) {
    kinfo("Initializing PIC...\n");
    
    // ICW1: Initialize
    outb(0x20, 0x11);  // Master PIC
    outb(0xA0, 0x11);  // Slave PIC
    
    // ICW2: Vector offset
    outb(0x21, 0x20);  // Master: interrupts 32-39
    outb(0xA1, 0x28);  // Slave: interrupts 40-47
    
    // ICW3: Cascade
    outb(0x21, 0x04);  // Master: IRQ2 connected to slave
    outb(0xA1, 0x02);  // Slave: connected to IRQ2
    
    // ICW4: Mode
    outb(0x21, 0x01);  // 8086 mode
    outb(0xA1, 0x01);  // 8086 mode
    
    // Mask ALL interrupts initially (including timer)
    // Timer will be unmasked after scheduler is initialized
    outb(0x21, 0xFF);  // Mask all master interrupts
    outb(0xA1, 0xFF);  // Mask all slave interrupts

    kinfo("PIC initialized (all IRQs masked)\n");
}

/**
 * Setup interrupt handlers in IDT
 */
void interrupts_init(void) {
    kinfo("Setting up interrupt handlers...\n");
    
    // Initialize PIC
    pic_init();
    
    // Register interrupt handlers in IDT
    // This will be done by idt_init() after we add the handlers
    // For now, we'll add them manually
    
    kinfo("Interrupt handlers ready\n");
}

