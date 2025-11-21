/**
 * @file timer.c
 * @brief ARM64 Generic Timer driver
 */

#include "../../include/types.h"
#include "../../include/hal/hal.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"

// Timer IRQ number (typically 30 for physical timer)
#define TIMER_IRQ 30

// Timer frequency (read from CNTFRQ_EL0)
static uint64_t timer_frequency = 0;
static void (*timer_callback)(void) = NULL;

static inline uint64_t read_cntfrq(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

static inline uint64_t read_cntpct(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(val));
    return val;
}

static inline void write_cntp_tval(uint32_t val) {
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"(val));
}

static inline void write_cntp_ctl(uint32_t val) {
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"(val));
}

static inline uint32_t read_cntp_ctl(void) {
    uint32_t val;
    __asm__ volatile("mrs %0, cntp_ctl_el0" : "=r"(val));
    return val;
}

error_code_t arm64_timer_init(void) {
    kinfo("ARM64 Generic Timer initialization...\n");

    // Read timer frequency
    timer_frequency = read_cntfrq();
    kinfo("Timer frequency: %lu Hz\n", timer_frequency);

    if (timer_frequency == 0) {
        kerror("Invalid timer frequency!\n");
        return ERR_HARDWARE_ERROR;
    }

    // Disable timer
    write_cntp_ctl(0);

    // Enable timer IRQ in GIC
    extern error_code_t arm64_gic_enable_irq(uint32_t irq);
    arm64_gic_enable_irq(TIMER_IRQ);

    kinfo("ARM64 Generic Timer initialized\n");
    return ERR_OK;
}

uint64_t arm64_timer_get_ticks(void) {
    return read_cntpct();
}

uint64_t arm64_timer_get_frequency(void) {
    return timer_frequency;
}

error_code_t arm64_timer_set_callback(void (*callback)(void)) {
    timer_callback = callback;

    if (callback) {
        // Set timer to fire every 10ms (100 Hz)
        uint32_t interval = timer_frequency / 100;
        write_cntp_tval(interval);

        // Enable timer with interrupt
        write_cntp_ctl(1);  // Enable bit
    } else {
        // Disable timer
        write_cntp_ctl(0);
    }

    return ERR_OK;
}

void arm64_timer_delay_us(uint64_t microseconds) {
    uint64_t start = read_cntpct();
    uint64_t ticks = (microseconds * timer_frequency) / 1000000;
    
    while ((read_cntpct() - start) < ticks) {
        __asm__ volatile("nop");
    }
}

// Timer IRQ handler
void arm64_timer_irq_handler(void) {
    // Acknowledge timer interrupt by writing to TVAL
    uint32_t interval = timer_frequency / 100;  // 10ms
    write_cntp_tval(interval);

    // Call registered callback
    if (timer_callback) {
        timer_callback();
    }
}
