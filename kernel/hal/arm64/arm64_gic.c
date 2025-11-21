/**
 * @file arm64_gic.c
 * @brief ARM64 Generic Interrupt Controller (GIC) Driver
 */

#include "arm64_hal.h"

// GIC Distributor registers
#define GICD_BASE 0x08000000
#define GICD_CTLR (GICD_BASE + 0x0000)
#define GICD_TYPER (GICD_BASE + 0x0004)
#define GICD_ISENABLER (GICD_BASE + 0x0100)
#define GICD_ICENABLER (GICD_BASE + 0x0180)
#define GICD_IPRIORITYR (GICD_BASE + 0x0400)
#define GICD_ITARGETSR (GICD_BASE + 0x0800)
#define GICD_ICFGR (GICD_BASE + 0x0C00)

// GIC CPU Interface registers
#define GICC_BASE 0x08010000
#define GICC_CTLR (GICC_BASE + 0x0000)
#define GICC_PMR (GICC_BASE + 0x0004)
#define GICC_IAR (GICC_BASE + 0x000C)
#define GICC_EOIR (GICC_BASE + 0x0010)

#define MAX_IRQS 256

static void (*irq_handlers[MAX_IRQS])(void);

static inline void mmio_write32(uint64_t addr, uint32_t val) {
    *(volatile uint32_t*)addr = val;
}

static inline uint32_t mmio_read32(uint64_t addr) {
    return *(volatile uint32_t*)addr;
}

void arm64_irq_init(void) {
    // Initialize handler table
    for (int i = 0; i < MAX_IRQS; i++) {
        irq_handlers[i] = NULL;
    }
    
    // Disable distributor
    mmio_write32(GICD_CTLR, 0);
    
    // Get number of interrupt lines
    uint32_t typer = mmio_read32(GICD_TYPER);
    uint32_t num_irqs = ((typer & 0x1F) + 1) * 32;
    
    // Disable all interrupts
    for (uint32_t i = 0; i < num_irqs / 32; i++) {
        mmio_write32(GICD_ICENABLER + i * 4, 0xFFFFFFFF);
    }
    
    // Set all priorities to default
    for (uint32_t i = 0; i < num_irqs; i++) {
        mmio_write32(GICD_IPRIORITYR + i, 0xA0);
    }
    
    // Route all interrupts to CPU 0
    for (uint32_t i = 32; i < num_irqs; i++) {
        mmio_write32(GICD_ITARGETSR + i, 0x01);
    }
    
    // Enable distributor
    mmio_write32(GICD_CTLR, 1);
    
    // Configure CPU interface
    mmio_write32(GICC_PMR, 0xFF);  // Set priority mask
    mmio_write32(GICC_CTLR, 1);    // Enable CPU interface
}

void arm64_irq_register(uint32_t irq, void (*handler)(void)) {
    if (irq < MAX_IRQS) {
        irq_handlers[irq] = handler;
        
        // Enable the interrupt
        uint32_t reg = irq / 32;
        uint32_t bit = irq % 32;
        uint32_t val = mmio_read32(GICD_ISENABLER + reg * 4);
        val |= (1 << bit);
        mmio_write32(GICD_ISENABLER + reg * 4, val);
    }
}

void arm64_irq_handler(void) {
    // Read interrupt ID
    uint32_t iar = mmio_read32(GICC_IAR);
    uint32_t irq = iar & 0x3FF;
    
    // Call handler if registered
    if (irq < MAX_IRQS && irq_handlers[irq]) {
        irq_handlers[irq]();
    }
    
    // End of interrupt
    mmio_write32(GICC_EOIR, iar);
}
