/**
 * @file apic.c
 * @brief Local APIC implementation
 */

#include "../../include/types.h"
#include "../../include/apic.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "../../include/config.h"

// Local APIC base address (virtual address after PHYS_MAP_BASE mapping)
static uint64_t lapic_base = 0;

/**
 * Read MSR (Model Specific Register)
 */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

/**
 * Write MSR
 */
static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

/**
 * Get Local APIC base address
 */
uint64_t apic_get_base(void) {
    return lapic_base;
}

/**
 * Read Local APIC register
 */
uint32_t apic_read(uint32_t reg) {
    return *(volatile uint32_t*)(lapic_base + reg);
}

/**
 * Write Local APIC register
 */
void apic_write(uint32_t reg, uint32_t value) {
    *(volatile uint32_t*)(lapic_base + reg) = value;
}

/**
 * Send EOI (End of Interrupt)
 */
void apic_send_eoi(void) {
    apic_write(LAPIC_EOI, 0);
}

/**
 * Send IPI (Inter-Processor Interrupt)
 */
void apic_send_ipi(uint32_t apic_id, uint32_t vector, uint32_t delivery_mode) {
    // Wait for any pending IPI to complete
    while (apic_read(LAPIC_ICR) & (1 << 12)) {
        __asm__ volatile("pause");
    }
    
    // Set destination in ICR2
    apic_write(LAPIC_ICR2, (apic_id << 24));
    
    // Set vector and delivery mode in ICR
    uint32_t icr = vector | delivery_mode | ICR_DEST_PHYSICAL | ICR_TRIGGER_EDGE;
    apic_write(LAPIC_ICR, icr);
    
    // Wait for IPI to be sent
    while (apic_read(LAPIC_ICR) & (1 << 12)) {
        __asm__ volatile("pause");
    }
}

/**
 * Send INIT IPI
 */
void apic_send_init(uint32_t apic_id) {
    apic_send_ipi(apic_id, 0, ICR_DELIVERY_INIT);
}

/**
 * Send STARTUP IPI
 */
void apic_send_startup(uint32_t apic_id, uint32_t vector) {
    apic_send_ipi(apic_id, vector, ICR_DELIVERY_STARTUP);
}

/**
 * Initialize Local APIC
 */
error_code_t apic_init(void) {
    kinfo("Initializing Local APIC...\n");

    // Read APIC base from MSR
    uint64_t msr_value = rdmsr(0x1B);  // IA32_APIC_BASE MSR
    uint64_t lapic_phys = msr_value & 0xFFFFF000;  // Physical base address (page-aligned)

    bool apic_enabled = (msr_value & (1 << 11)) != 0;  // APIC Global Enable bit

    if (!apic_enabled) {
        // Enable APIC
        msr_value |= (1 << 11);
        wrmsr(0x1B, msr_value);
        lapic_phys = msr_value & 0xFFFFF000;
        kinfo("APIC was disabled, enabled it\n");
    }

    // Map physical APIC address to virtual address using PHYS_MAP_BASE
    lapic_base = lapic_phys + PHYS_MAP_BASE;

    kinfo("Local APIC physical base: 0x%016lx\n", lapic_phys);
    kinfo("Local APIC virtual base: 0x%016lx\n", lapic_base);
    
    // Read APIC version
    uint32_t version = apic_read(LAPIC_VER);
    uint32_t max_lvt = (version >> 16) & 0xFF;
    uint32_t apic_id = (apic_read(LAPIC_ID) >> 24) & 0xFF;
    
    kinfo("Local APIC ID: %u, Version: 0x%08x, Max LVT: %u\n", apic_id, version, max_lvt);
    
    // Enable Local APIC (set SVR)
    apic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | LAPIC_SVR_VECTOR);
    
    // Set Task Priority Register to 0 (allow all interrupts)
    apic_write(LAPIC_TPR, 0);
    
    kinfo("Local APIC initialized\n");
    return ERR_OK;
}

