/**
 * @file arm64_timer.c
 * @brief ARM64 Generic Timer Driver
 */

#include "arm64_hal.h"

// Timer registers
#define CNTFRQ_EL0 "cntfrq_el0"  // Counter frequency
#define CNTPCT_EL0 "cntpct_el0"  // Physical count
#define CNTP_CTL_EL0 "cntp_ctl_el0"  // Timer control
#define CNTP_CVAL_EL0 "cntp_cval_el0"  // Timer compare value
#define CNTP_TVAL_EL0 "cntp_tval_el0"  // Timer value

// Timer control bits
#define CNTP_CTL_ENABLE (1 << 0)
#define CNTP_CTL_IMASK (1 << 1)
#define CNTP_CTL_ISTATUS (1 << 2)

static uint64_t timer_frequency = 0;
static uint64_t ticks_per_us = 0;

// Read system register
static inline uint64_t read_sysreg_timer(const char* reg) {
    uint64_t val;
    if (strcmp(reg, CNTFRQ_EL0) == 0) {
        __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(val));
    } else if (strcmp(reg, CNTPCT_EL0) == 0) {
        __asm__ volatile("mrs %0, cntpct_el0" : "=r"(val));
    } else if (strcmp(reg, CNTP_CTL_EL0) == 0) {
        __asm__ volatile("mrs %0, cntp_ctl_el0" : "=r"(val));
    } else if (strcmp(reg, CNTP_TVAL_EL0) == 0) {
        __asm__ volatile("mrs %0, cntp_tval_el0" : "=r"(val));
    }
    return val;
}

// Write system register
static inline void write_sysreg_timer(const char* reg, uint64_t val) {
    if (strcmp(reg, CNTP_CTL_EL0) == 0) {
        __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"(val));
    } else if (strcmp(reg, CNTP_CVAL_EL0) == 0) {
        __asm__ volatile("msr cntp_cval_el0, %0" :: "r"(val));
    } else if (strcmp(reg, CNTP_TVAL_EL0) == 0) {
        __asm__ volatile("msr cntp_tval_el0, %0" :: "r"(val));
    }
    __asm__ volatile("isb");
}

void arm64_timer_init(void) {
    // Read timer frequency
    timer_frequency = read_sysreg_timer(CNTFRQ_EL0);
    ticks_per_us = timer_frequency / 1000000;
    
    // Disable timer
    write_sysreg_timer(CNTP_CTL_EL0, 0);
    
    printf("ARM64 Timer: frequency=%lu Hz, ticks_per_us=%lu\n",
           timer_frequency, ticks_per_us);
}

uint64_t arm64_timer_get_ticks(void) {
    return read_sysreg_timer(CNTPCT_EL0);
}

void arm64_timer_set_interval(uint64_t interval_us) {
    // Calculate ticks for interval
    uint64_t ticks = interval_us * ticks_per_us;
    
    // Set timer value
    write_sysreg_timer(CNTP_TVAL_EL0, ticks);
    
    // Enable timer
    write_sysreg_timer(CNTP_CTL_EL0, CNTP_CTL_ENABLE);
}

void arm64_timer_disable(void) {
    write_sysreg_timer(CNTP_CTL_EL0, 0);
}

uint64_t arm64_timer_get_frequency(void) {
    return timer_frequency;
}

void arm64_timer_delay_us(uint64_t microseconds) {
    uint64_t start = arm64_timer_get_ticks();
    uint64_t ticks = microseconds * ticks_per_us;
    
    while ((arm64_timer_get_ticks() - start) < ticks) {
        __asm__ volatile("nop");
    }
}

void arm64_timer_irq_handler(void) {
    // Disable timer interrupt
    uint64_t ctl = read_sysreg_timer(CNTP_CTL_EL0);
    ctl |= CNTP_CTL_IMASK;
    write_sysreg_timer(CNTP_CTL_EL0, ctl);
    
    // Call scheduler tick
    extern void scheduler_tick(void);
    scheduler_tick();
    
    // Re-enable timer for next tick
    arm64_timer_set_interval(1000); // 1ms tick
}
