/**
 * @file ps2.c
 * @brief PS/2 controller driver
 */

#include "../../include/drivers/ps2.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"

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
 * Wait for output buffer to be ready (bit 0 of status = 1)
 */
error_code_t ps2_wait_output(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (ps2_read_status() & PS2_STATUS_OUTPUT) {
            return ERR_OK;
        }
    }
    return ERR_TIMEOUT;
}

/**
 * Wait for input buffer to be empty (bit 1 of status = 0)
 */
error_code_t ps2_wait_input(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (!(ps2_read_status() & PS2_STATUS_INPUT)) {
            return ERR_OK;
        }
    }
    return ERR_TIMEOUT;
}

/**
 * Read data from PS/2 data port
 */
uint8_t ps2_read_data(void) {
    return inb(PS2_DATA_PORT);
}

/**
 * Write data to PS/2 data port
 */
void ps2_write_data(uint8_t data) {
    outb(PS2_DATA_PORT, data);
}

/**
 * Read status from PS/2 status port
 */
uint8_t ps2_read_status(void) {
    return inb(PS2_STATUS_PORT);
}

/**
 * Write command to PS/2 command port
 */
void ps2_write_command(uint8_t cmd) {
    outb(PS2_COMMAND_PORT, cmd);
}

/**
 * Send byte to PS/2 device and wait for ACK
 */
error_code_t ps2_send_byte(uint8_t data) {
    // Wait for input buffer to be empty
    if (ps2_wait_input() != ERR_OK) {
        return ERR_TIMEOUT;
    }
    
    // Write data
    ps2_write_data(data);
    
    // Wait for ACK
    if (ps2_wait_output() != ERR_OK) {
        return ERR_TIMEOUT;
    }
    
    uint8_t response = ps2_read_data();
    if (response == PS2_RESP_ACK) {
        return ERR_OK;
    } else if (response == PS2_RESP_RESEND) {
        return ERR_AGAIN;
    } else {
        return ERR_FAILED;
    }
}

/**
 * Initialize PS/2 controller
 */
error_code_t ps2_init(void) {
    kinfo("Initializing PS/2 controller...\n");
    
    // Disable both devices
    ps2_write_command(PS2_CMD_DISABLE_PORT1);
    ps2_write_command(PS2_CMD_DISABLE_PORT2);
    
    // Flush output buffer
    while (ps2_read_status() & PS2_STATUS_OUTPUT) {
        ps2_read_data();
    }
    
    // Read configuration byte
    ps2_write_command(PS2_CMD_READ_CONFIG);
    if (ps2_wait_output() != ERR_OK) {
        kwarn("PS/2: Failed to read configuration\n");
        return ERR_DEVICE_NOT_FOUND;
    }
    
    uint8_t config = ps2_read_data();
    kinfo("PS/2 configuration: 0x%02x\n", config);
    
    // Disable interrupts and translation
    config &= ~0x03;  // Clear interrupt enable bits
    config &= ~0x40;  // Clear translation bit
    
    // Write configuration back
    ps2_write_command(PS2_CMD_WRITE_CONFIG);
    if (ps2_wait_input() != ERR_OK) {
        return ERR_TIMEOUT;
    }
    ps2_write_data(config);
    
    // Test controller
    ps2_write_command(PS2_CMD_TEST_CONTROLLER);
    if (ps2_wait_output() != ERR_OK) {
        return ERR_TIMEOUT;
    }
    
    uint8_t test_result = ps2_read_data();
    if (test_result != PS2_RESP_TEST_OK) {
        kwarn("PS/2 controller test failed: 0x%02x\n", test_result);
        return ERR_FAILED;
    }
    
    // Test port 1 (keyboard)
    ps2_write_command(PS2_CMD_TEST_PORT1);
    if (ps2_wait_output() != ERR_OK) {
        return ERR_TIMEOUT;
    }
    
    test_result = ps2_read_data();
    bool port1_ok = (test_result == 0x00);
    
    // Test port 2 (mouse) if available
    bool port2_ok = false;
    ps2_write_command(PS2_CMD_TEST_PORT2);
    if (ps2_wait_output() == ERR_OK) {
        test_result = ps2_read_data();
        port2_ok = (test_result == 0x00);
    }
    
    kinfo("PS/2: Port 1 (keyboard): %s\n", port1_ok ? "OK" : "Failed");
    kinfo("PS/2: Port 2 (mouse): %s\n", port2_ok ? "OK" : "Failed");
    
    // Enable interrupts for working ports
    config = ps2_read_status();
    ps2_write_command(PS2_CMD_READ_CONFIG);
    if (ps2_wait_output() == ERR_OK) {
        config = ps2_read_data();
        if (port1_ok) {
            config |= 0x01;  // Enable port 1 interrupt
        }
        if (port2_ok) {
            config |= 0x02;  // Enable port 2 interrupt
        }
        
        ps2_write_command(PS2_CMD_WRITE_CONFIG);
        if (ps2_wait_input() == ERR_OK) {
            ps2_write_data(config);
        }
    }
    
    // Enable devices
    if (port1_ok) {
        ps2_write_command(PS2_CMD_ENABLE_PORT1);
    }
    if (port2_ok) {
        ps2_write_command(PS2_CMD_ENABLE_PORT2);
    }
    
    kinfo("PS/2 controller initialized\n");
    return ERR_OK;
}

