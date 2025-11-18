/**
 * @file apic.h
 * @brief Advanced Programmable Interrupt Controller (APIC) interface
 */

#ifndef KERNEL_APIC_H
#define KERNEL_APIC_H

#include "types.h"
#include "errors.h"

// Local APIC registers
#define LAPIC_ID          0x020
#define LAPIC_VER         0x030
#define LAPIC_TPR         0x080
#define LAPIC_APR         0x090
#define LAPIC_PPR         0x0A0
#define LAPIC_EOI         0x0B0
#define LAPIC_RRR         0x0C0
#define LAPIC_LDR         0x0D0
#define LAPIC_DFR         0x0E0
#define LAPIC_SVR         0x0F0
#define LAPIC_ISR         0x100
#define LAPIC_TMR         0x180
#define LAPIC_IRR         0x200
#define LAPIC_ESR         0x280
#define LAPIC_ICR         0x300
#define LAPIC_ICR2        0x310
#define LAPIC_LVTT        0x320
#define LAPIC_LVTTHMR     0x330
#define LAPIC_LVTPC       0x340
#define LAPIC_LVT0        0x350
#define LAPIC_LVT1        0x360
#define LAPIC_LVTERR      0x370
#define LAPIC_TICR        0x380
#define LAPIC_TCCR        0x390
#define LAPIC_TDCR        0x3E0

// Local APIC SVR (Spurious Interrupt Vector Register)
#define LAPIC_SVR_ENABLE  (1 << 8)
#define LAPIC_SVR_VECTOR  0xFF

// Local APIC ICR (Interrupt Command Register) delivery modes
#define ICR_DELIVERY_FIXED        0x000
#define ICR_DELIVERY_LOWEST       0x001
#define ICR_DELIVERY_SMI          0x002
#define ICR_DELIVERY_NMI          0x004
#define ICR_DELIVERY_INIT         0x005
#define ICR_DELIVERY_STARTUP      0x006

// Local APIC ICR destination modes
#define ICR_DEST_PHYSICAL         0x000
#define ICR_DEST_LOGICAL          0x008

// Local APIC ICR trigger modes
#define ICR_TRIGGER_EDGE          0x000
#define ICR_TRIGGER_LEVEL         0x080

// Local APIC ICR destination shorthand
#define ICR_SHORTHAND_NONE        0x000
#define ICR_SHORTHAND_SELF        0x400
#define ICR_SHORTHAND_ALL         0x800
#define ICR_SHORTHAND_OTHERS      0xC00

// APIC functions
error_code_t apic_init(void);
uint32_t apic_read(uint32_t reg);
void apic_write(uint32_t reg, uint32_t value);
void apic_send_eoi(void);
void apic_send_ipi(uint32_t apic_id, uint32_t vector, uint32_t delivery_mode);
void apic_send_init(uint32_t apic_id);
void apic_send_startup(uint32_t apic_id, uint32_t vector);

// Get Local APIC base address
uint64_t apic_get_base(void);

#endif // KERNEL_APIC_H

