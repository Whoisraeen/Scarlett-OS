/**
 * @file serial.c
 * @brief Serial port driver for x86_64 (COM1)
 * 
 * This driver provides serial console output for debugging.
 * Uses COM1 (0x3F8) at 38400 baud.
 */

#include "../../include/types.h"

// COM1 port base address
#define COM1_PORT 0x3F8

// COM port registers (offset from base)
#define COM_DATA_REG    0  // Data register (R/W)
#define COM_IER_REG     1  // Interrupt Enable Register
#define COM_IIR_REG     2  // Interrupt Identification Register
#define COM_LCR_REG     3  // Line Control Register
#define COM_MCR_REG     4  // Modem Control Register
#define COM_LSR_REG     5  // Line Status Register
#define COM_MSR_REG     6  // Modem Status Register
#define COM_SCRATCH_REG 7  // Scratch Register

// Line Status Register bits
#define LSR_DATA_READY    (1 << 0)
#define LSR_OVERRUN_ERROR (1 << 1)
#define LSR_PARITY_ERROR  (1 << 2)
#define LSR_FRAMING_ERROR (1 << 3)
#define LSR_BREAK_INT     (1 << 4)
#define LSR_THR_EMPTY     (1 << 5)  // Transmitter Holding Register Empty
#define LSR_TRANSMITTER_EMPTY (1 << 6)

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
 * Wait for transmit buffer to be empty
 */
static void serial_wait_transmit(void) {
    while ((inb(COM1_PORT + COM_LSR_REG) & LSR_THR_EMPTY) == 0) {
        __asm__ volatile("pause");
    }
}

/**
 * Initialize serial port
 */
void serial_init(void) {
    // Disable interrupts
    outb(COM1_PORT + COM_IER_REG, 0x00);
    
    // Enable DLAB (set baud rate divisor)
    outb(COM1_PORT + COM_LCR_REG, 0x80);
    
    // Set divisor to 3 (38400 baud)
    outb(COM1_PORT + COM_DATA_REG, 0x03);
    outb(COM1_PORT + COM_IER_REG, 0x00);
    
    // 8 bits, no parity, one stop bit
    outb(COM1_PORT + COM_LCR_REG, 0x03);
    
    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1_PORT + COM_IIR_REG, 0xC7);
    
    // IRQs enabled, RTS/DSR set
    outb(COM1_PORT + COM_MCR_REG, 0x0B);
    
    // Test serial port (loopback test)
    outb(COM1_PORT + COM_MCR_REG, 0x1E);
    outb(COM1_PORT + COM_DATA_REG, 0xAE);
    
    if (inb(COM1_PORT + COM_DATA_REG) != 0xAE) {
        // Serial port is faulty, but we'll continue anyway
        // In a real OS, you might want to disable serial output
    }
    
    // Set normal operation mode
    outb(COM1_PORT + COM_MCR_REG, 0x0F);
}

/**
 * Write a character to serial port
 */
void serial_putc(char c) {
    // Convert \n to \r\n for proper terminal display
    if (c == '\n') {
        serial_wait_transmit();
        outb(COM1_PORT + COM_DATA_REG, '\r');
    }
    
    serial_wait_transmit();
    outb(COM1_PORT + COM_DATA_REG, c);
}

/**
 * Write a string to serial port
 */
void serial_puts(const char* str) {
    if (!str) return;
    
    while (*str) {
        serial_putc(*str++);
    }
}

/**
 * Check if serial port has data available
 */
bool serial_has_data(void) {
    return (inb(COM1_PORT + COM_LSR_REG) & LSR_DATA_READY) != 0;
}

/**
 * Read a character from serial port (blocking)
 */
char serial_getc(void) {
    while (!serial_has_data()) {
        __asm__ volatile("pause");
    }
    return inb(COM1_PORT + COM_DATA_REG);
}

