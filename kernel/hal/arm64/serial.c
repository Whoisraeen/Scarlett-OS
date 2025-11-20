/**
 * @file serial.c
 * @brief ARM64 serial/UART driver (PL011 for QEMU virt)
 */

#include "../../include/types.h"
#include "../../include/hal/hal.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"

// PL011 UART registers (QEMU virt machine)
#define PL011_UART_BASE 0x9000000

// PL011 register offsets
#define PL011_UARTDR    0x000  // Data register
#define PL011_UARTFR    0x018  // Flag register
#define PL011_UARTIBRD  0x024  // Integer baud rate divisor
#define PL011_UARTFBRD  0x028  // Fractional baud rate divisor
#define PL011_UARTLCR_H 0x02C  // Line control register
#define PL011_UARTCR    0x030  // Control register
#define PL011_UARTIMSC  0x038  // Interrupt mask set/clear
#define PL011_UARTICR   0x044  // Interrupt clear register

// Flag register bits
#define PL011_UARTFR_TXFF (1 << 5)  // Transmit FIFO full
#define PL011_UARTFR_RXFE (1 << 4)  // Receive FIFO empty

// Control register bits
#define PL011_UARTCR_UARTEN (1 << 0)  // UART enable
#define PL011_UARTCR_TXE     (1 << 8)  // Transmit enable
#define PL011_UARTCR_RXE     (1 << 9)  // Receive enable

static volatile uint32_t* uart_base = (volatile uint32_t*)(PL011_UART_BASE);

/**
 * Initialize PL011 UART
 */
error_code_t hal_serial_init(void) {
    if (!uart_base) {
        return ERR_INVALID_PARAM;
    }
    
    // Disable UART
    uart_base[PL011_UARTCR / 4] = 0;
    
    // Set baud rate to 115200 (assuming 24MHz clock)
    // Divisor = 24000000 / (16 * 115200) = 13.02
    uart_base[PL011_UARTIBRD / 4] = 13;
    uart_base[PL011_UARTFBRD / 4] = 1;
    
    // 8 bits, no parity, 1 stop bit
    uart_base[PL011_UARTLCR_H / 4] = 0x70;
    
    // Enable UART, TX, and RX
    uart_base[PL011_UARTCR / 4] = PL011_UARTCR_UARTEN | PL011_UARTCR_TXE | PL011_UARTCR_RXE;
    
    // Clear interrupts
    uart_base[PL011_UARTICR / 4] = 0x7FF;
    
    return ERR_OK;
}

/**
 * Write a character to UART
 */
void hal_serial_write_char(char c) {
    if (!uart_base) return;
    
    // Wait for transmit FIFO to have space
    while (uart_base[PL011_UARTFR / 4] & PL011_UARTFR_TXFF) {
        // Busy wait
    }
    
    // Write character
    uart_base[PL011_UARTDR / 4] = (uint32_t)c;
}

/**
 * Read a character from UART (non-blocking)
 * @return Character or -1 if no data available
 */
int hal_serial_read_char(void) {
    if (!uart_base) return -1;
    
    // Check if data is available
    if (uart_base[PL011_UARTFR / 4] & PL011_UARTFR_RXFE) {
        return -1;  // No data
    }
    
    return (int)(uart_base[PL011_UARTDR / 4] & 0xFF);
}

