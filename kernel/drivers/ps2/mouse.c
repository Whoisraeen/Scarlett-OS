/**
 * @file mouse.c
 * @brief PS/2 mouse driver
 */

#include "../../include/drivers/mouse.h"
#include "../../include/drivers/ps2.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"

// Mouse state
static struct {
    bool initialized;
    uint8_t packet[4];
    uint8_t packet_index;
    bool expecting_ack;
    mouse_callback_t callback;
    int8_t x_overflow;
    int8_t y_overflow;
} mouse_state = {0};

// Forward declarations
extern error_code_t ps2_wait_output(void);
extern uint8_t ps2_read_data(void);
extern error_code_t ps2_send_byte(uint8_t data);

/**
 * Handle mouse interrupt
 */
void mouse_interrupt_handler(void) {
    if (!mouse_state.initialized) {
        return;
    }
    
    // Read byte from mouse
    uint8_t data = ps2_read_data();
    
    // Handle ACK responses
    if (mouse_state.expecting_ack) {
        mouse_state.expecting_ack = false;
        if (data == PS2_RESP_ACK) {
            // Command acknowledged
        }
        return;
    }
    
    // PS/2 mouse sends 3-byte packets (4-byte for scroll wheel)
    if (mouse_state.packet_index == 0) {
        // First byte: button states and flags
        mouse_state.packet[0] = data;
        mouse_state.packet_index = 1;
    } else if (mouse_state.packet_index == 1) {
        // Second byte: X movement (signed)
        mouse_state.packet[1] = data;
        mouse_state.packet_index = 2;
    } else if (mouse_state.packet_index == 2) {
        // Third byte: Y movement (signed)
        mouse_state.packet[2] = data;
        
        // Parse packet
        uint8_t flags = mouse_state.packet[0];
        int8_t x_movement = (int8_t)mouse_state.packet[1];
        int8_t y_movement = (int8_t)mouse_state.packet[2];
        
        // Check for overflow
        bool x_overflow = (flags & 0x40) != 0;
        bool y_overflow = (flags & 0x80) != 0;
        
        // Adjust for overflow
        if (x_overflow) {
            x_movement = (x_movement < 0) ? -128 : 127;
        }
        if (y_overflow) {
            y_movement = (y_movement < 0) ? -128 : 127;
        }
        
        // Get button states
        bool left = (flags & 0x01) != 0;
        bool right = (flags & 0x02) != 0;
        bool middle = (flags & 0x04) != 0;
        
        // Invert Y (mouse Y is inverted)
        y_movement = -y_movement;
        
        // Create mouse event
        if (mouse_state.callback) {
            mouse_event_t event = {0};
            event.x = x_movement;
            event.y = y_movement;
            event.scroll = 0;  // Scroll wheel not supported in standard PS/2
            event.buttons = flags & 0x07;
            event.button_left = left;
            event.button_right = right;
            event.button_middle = middle;
            
            mouse_state.callback(&event);
        }
        
        // Reset packet index
        mouse_state.packet_index = 0;
    }
}

/**
 * Set mouse callback
 */
error_code_t mouse_set_callback(mouse_callback_t callback) {
    mouse_state.callback = callback;
    return ERR_OK;
}

/**
 * Initialize mouse
 */
error_code_t mouse_init(void) {
    kinfo("Initializing PS/2 mouse...\n");
    
    // Send command to second PS/2 port (mouse)
    ps2_write_command(0xD4);  // Write to second PS/2 port
    
    // Reset mouse
    error_code_t err = ps2_send_byte(0xFF);  // Reset command
    if (err != ERR_OK && err != ERR_AGAIN) {
        kwarn("Mouse reset failed\n");
        return err;
    }
    
    // Wait for self-test response (0xAA)
    if (ps2_wait_output() == ERR_OK) {
        uint8_t response = ps2_read_data();
        if (response == PS2_RESP_SELF_TEST_OK) {
            kinfo("Mouse self-test OK\n");
        } else {
            kwarn("Mouse self-test failed: 0x%02x\n", response);
            return ERR_DEVICE_NOT_FOUND;
        }
    }
    
    // Enable mouse data reporting
    ps2_write_command(0xD4);  // Write to second PS/2 port
    err = ps2_send_byte(0xF4);  // Enable data reporting
    if (err != ERR_OK && err != ERR_AGAIN) {
        kwarn("Failed to enable mouse data reporting\n");
    }
    
    mouse_state.initialized = true;
    mouse_state.packet_index = 0;
    kinfo("PS/2 mouse initialized\n");
    
    return ERR_OK;
}

