/**
 * @file gic.c
 * @brief ARM64 GIC (Generic Interrupt Controller) driver
 */

#include "../../include/types.h"
#include "../../include/hal/hal.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"

// GIC Distributor registers
#define GICD_CTLR       0x0000
#define GICD_TYPER      0x0004
#define GICD_ISENABLER  0x0100
#define GICD_ICENABLER  0x0180
#define GICD_ISPENDR    0x0200
#define GICD_ICPENDR    0x0280
#define GICD_IPRIORITYR 0x0400
#define GICD_ITARGETSR  0x0800
#define GICD_ICFGR      0x0C00

// GIC CPU Interface registers
#define GICC_CTLR       0x0000
#define GICC_PMR        0x0004
#define GICC_BPR        0x0008
#define GICC_IAR        0x000C
#define GICC_EOIR       0x0010
#define GICC_RPR        0x0014
#define GICC_HPPIR      0x0018

// GIC base addresses (from device tree or hardcoded for QEMU)
static volatile uint32_t* gicd_base = (volatile uint32_t*)0x08000000;  // Distributor
static volatile uint32_t* gicc_base = (volatile uint32_t*)0x08010000;  // CPU Interface

static uint32_t gic_num_irqs = 0;

static inline uint32_t gicd_read(uint32_t offset) {
    return *(volatile uint32_t*)((uint8_t*)gicd_base + offset);
}

static inline void gicd_write(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)((uint8_t*)gicd_base + offset) = value;
}

static inline uint32_t gicc_read(uint32_t offset) {
    return *(volatile uint32_t*)((uint8_t*)gicc_base + offset);
}

static inline void gicc_write(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)((uint8_t*)gicc_base + offset) = value;
}

error_code_t arm64_gic_init(void) {
    kinfo("ARM64 GIC initialization...\n");

    // Read GIC type to determine number of IRQs
    uint32_t typer = gicd_read(GICD_TYPER);
    gic_num_irqs = ((typer & 0x1F) + 1) * 32;
    kinfo("GIC supports %u interrupts\n", gic_num_irqs);

    // Disable distributor
    gicd_write(GICD_CTLR, 0);

    // Disable all interrupts
    for (uint32_t i = 0; i < gic_num_irqs; i += 32) {
        gicd_write(GICD_ICENABLER + (i / 8), 0xFFFFFFFF);
    }

    // Clear all pending interrupts
    for (uint32_t i = 0; i < gic_num_irqs; i += 32) {
        gicd_write(GICD_ICPENDR + (i / 8), 0xFFFFFFFF);
    }

    // Set all interrupts to lowest priority
    for (uint32_t i = 0; i < gic_num_irqs; i += 4) {
        gicd_write(GICD_IPRIORITYR + i, 0xA0A0A0A0);
    }

    // Route all SPIs to CPU 0
    for (uint32_t i = 32; i < gic_num_irqs; i += 4) {
        gicd_write(GICD_ITARGETSR + i, 0x01010101);
    }

    // Configure all interrupts as level-sensitive
    for (uint32_t i = 0; i < gic_num_irqs; i += 16) {
        gicd_write(GICD_ICFGR + (i / 4), 0);
    }

    // Enable distributor
    gicd_write(GICD_CTLR, 1);

    // Configure CPU interface
    gicc_write(GICC_PMR, 0xF0);  // Set priority mask (allow all priorities)
    gicc_write(GICC_BPR, 0);     // No priority grouping
    gicc_write(GICC_CTLR, 1);    // Enable CPU interface

    kinfo("GIC initialization complete\n");
    return ERR_OK;
}

error_code_t arm64_gic_enable_irq(uint32_t irq) {
    if (irq >= gic_num_irqs) {
        return ERR_INVALID_PARAMETER;
    }

    uint32_t reg = irq / 32;
    uint32_t bit = irq % 32;
    gicd_write(GICD_ISENABLER + (reg * 4), 1 << bit);

    return ERR_OK;
}

error_code_t arm64_gic_disable_irq(uint32_t irq) {
    if (irq >= gic_num_irqs) {
        return ERR_INVALID_PARAMETER;
    }

    uint32_t reg = irq / 32;
    uint32_t bit = irq % 32;
    gicd_write(GICD_ICENABLER + (reg * 4), 1 << bit);

    return ERR_OK;
}

uint32_t arm64_gic_acknowledge_irq(void) {
    return gicc_read(GICC_IAR) & 0x3FF;  // IRQ number is in bits [9:0]
}

void arm64_gic_end_of_interrupt(uint32_t irq) {
    gicc_write(GICC_EOIR, irq);
}

// IRQ handler called from vectors.S
void arm64_irq_handler(void) {
    uint32_t irq = arm64_gic_acknowledge_irq();

    if (irq >= 1020) {
        // Spurious interrupt
        return;
    }

    // Call registered IRQ handler
    extern void irq_call_handlers(uint8_t irq);
    if (irq < 64) {  // Only call for valid IRQ range
        irq_call_handlers((uint8_t)irq);
    }
    
    arm64_gic_end_of_interrupt(irq);
}
