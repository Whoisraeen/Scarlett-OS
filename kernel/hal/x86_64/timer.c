/**
 * @file timer.c
 * @brief Programmable Interval Timer (PIT) driver
 * 
 * Sets up timer interrupts for preemptive multitasking.
 */

#include "../../include/types.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"

// PIT ports
#define PIT_CHANNEL0_DATA 0x40
#define PIT_CHANNEL1_DATA 0x41
#define PIT_CHANNEL2_DATA 0x42
#define PIT_COMMAND       0x43

// PIT command bits
#define PIT_CHANNEL0      (0 << 6)
#define PIT_ACCESS_LOHI   (3 << 4)  // Low byte, then high byte
#define PIT_MODE_3        (3 << 1)  // Square wave generator
#define PIT_BINARY        (0 << 0)  // Binary mode

// PIT frequency: 1193182 Hz
#define PIT_FREQUENCY     1193182

// Target frequency: 100 Hz (10ms per tick)
#define TARGET_FREQUENCY  100

// Calculate divisor
#define PIT_DIVISOR       (PIT_FREQUENCY / TARGET_FREQUENCY)

// Timer tick counter
static volatile uint64_t timer_ticks = 0;

/**
 * Read byte from I/O port
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * Write byte to I/O port
 */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/**
 * Initialize PIT timer (but keep IRQ masked initially)
 */
void timer_init(void) {
    kinfo("Initializing PIT timer...\n");

    // Calculate divisor
    uint16_t divisor = (uint16_t)PIT_DIVISOR;

    kinfo("PIT: Setting frequency to %u Hz (divisor: %u)\n", TARGET_FREQUENCY, divisor);

    // Send command byte
    outb(PIT_COMMAND, PIT_CHANNEL0 | PIT_ACCESS_LOHI | PIT_MODE_3 | PIT_BINARY);

    // Set divisor (low byte, then high byte)
    outb(PIT_CHANNEL0_DATA, divisor & 0xFF);
    outb(PIT_CHANNEL0_DATA, (divisor >> 8) & 0xFF);

    timer_ticks = 0;

    // Note: Timer IRQ (IRQ 0) is masked by default in PIC init
    // It will be unmasked when scheduler is ready

    kinfo("PIT timer initialized (IRQ masked, 10ms per tick)\n");
}

/**
 * Get current tick count
 */
uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

/**
 * Get time in milliseconds
 */
uint64_t timer_get_ms(void) {
    return timer_ticks * (1000 / TARGET_FREQUENCY);
}

// Flag to indicate if scheduler is ready
static bool scheduler_ready = false;

/**
 * Enable scheduler ticks (unmasks timer IRQ)
 */
void timer_enable_scheduler(void) {
    scheduler_ready = true;

    // Unmask IRQ 0 (timer) on PIC
    // Note: Interrupts are already enabled, so first interrupt will fire immediately
    uint8_t mask = inb(0x21);
    mask &= ~(1 << 0);  // Clear bit 0 to unmask IRQ 0
    outb(0x21, mask);

    // Give the CPU a moment to process any pending interrupts
    __asm__ volatile("nop; nop; nop;");
}

// Timer callback function pointer
static void (*timer_callback)(void) = NULL;

/**
 * Set timer callback (for scheduler ticks)
 */
void timer_set_callback(void (*callback)(void)) {
    timer_callback = callback;
    // Enable scheduler ticks when callback is set
    timer_enable_scheduler();
}

/**
 * Timer interrupt handler (called from interrupt handler)
 */
void timer_interrupt_handler(void) {
    timer_ticks++;

    // Call scheduler tick if scheduler is ready
    // NOTE: Do NOT use kprintf/kinfo here - we're in interrupt context!
    if (scheduler_ready) {
        extern void scheduler_tick(void);
        scheduler_tick();
    }
    
    // Call registered callback if set
    if (timer_callback) {
        timer_callback();
    }
}

/**
 * Sleep for specified milliseconds (busy wait)
 *
 * Note: This is a simple busy-wait implementation.
 * For proper sleep, threads should block and be woken by timer.
 */
void timer_sleep_ms(uint64_t ms) {
    uint64_t start_ticks = timer_ticks;
    uint64_t target_ticks = start_ticks + (ms * TARGET_FREQUENCY / 1000);

    while (timer_ticks < target_ticks) {
        __asm__ volatile("pause");
    }
}

