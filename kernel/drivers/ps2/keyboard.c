/**
 * @file keyboard.c
 * @brief PS/2 keyboard driver
 */

#include "../../include/drivers/keyboard.h"
#include "../../include/drivers/ps2.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "../../include/string.h"

// Forward declaration
extern error_code_t ps2_wait_output(void);
extern uint8_t ps2_read_data(void);

// Keyboard state
static struct {
    bool initialized;
    bool shift_pressed;
    bool ctrl_pressed;
    bool alt_pressed;
    bool caps_lock;
    bool num_lock;
    bool scroll_lock;
    bool extended_scancode;
    uint8_t last_scancode;
    keyboard_callback_t callback;
} keyboard_state = {0};

// Scancode set 1 to ASCII conversion (lowercase, no shift)
static const char scancode_to_ascii_normal[128] = {
    [0x01] = 0,     // ESC
    [0x02] = '1',
    [0x03] = '2',
    [0x04] = '3',
    [0x05] = '4',
    [0x06] = '5',
    [0x07] = '6',
    [0x08] = '7',
    [0x09] = '8',
    [0x0A] = '9',
    [0x0B] = '0',
    [0x0C] = '-',
    [0x0D] = '=',
    [0x0E] = '\b',  // Backspace
    [0x0F] = '\t',  // Tab
    [0x10] = 'q',
    [0x11] = 'w',
    [0x12] = 'e',
    [0x13] = 'r',
    [0x14] = 't',
    [0x15] = 'y',
    [0x16] = 'u',
    [0x17] = 'i',
    [0x18] = 'o',
    [0x19] = 'p',
    [0x1A] = '[',
    [0x1B] = ']',
    [0x1C] = '\n',  // Enter
    [0x1E] = 'a',
    [0x1F] = 's',
    [0x20] = 'd',
    [0x21] = 'f',
    [0x22] = 'g',
    [0x23] = 'h',
    [0x24] = 'j',
    [0x25] = 'k',
    [0x26] = 'l',
    [0x27] = ';',
    [0x28] = '\'',
    [0x29] = '`',
    [0x2B] = '\\',
    [0x2C] = 'z',
    [0x2D] = 'x',
    [0x2E] = 'c',
    [0x2F] = 'v',
    [0x30] = 'b',
    [0x31] = 'n',
    [0x32] = 'm',
    [0x33] = ',',
    [0x34] = '.',
    [0x35] = '/',
    [0x39] = ' ',   // Space
};

// Scancode set 1 to ASCII conversion (shifted)
static const char scancode_to_ascii_shift[128] = {
    [0x02] = '!',
    [0x03] = '@',
    [0x04] = '#',
    [0x05] = '$',
    [0x06] = '%',
    [0x07] = '^',
    [0x08] = '&',
    [0x09] = '*',
    [0x0A] = '(',
    [0x0B] = ')',
    [0x0C] = '_',
    [0x0D] = '+',
    [0x10] = 'Q',
    [0x11] = 'W',
    [0x12] = 'E',
    [0x13] = 'R',
    [0x14] = 'T',
    [0x15] = 'Y',
    [0x16] = 'U',
    [0x17] = 'I',
    [0x18] = 'O',
    [0x19] = 'P',
    [0x1A] = '{',
    [0x1B] = '}',
    [0x1E] = 'A',
    [0x1F] = 'S',
    [0x20] = 'D',
    [0x21] = 'F',
    [0x22] = 'G',
    [0x23] = 'H',
    [0x24] = 'J',
    [0x25] = 'K',
    [0x26] = 'L',
    [0x27] = ':',
    [0x28] = '"',
    [0x29] = '~',
    [0x2B] = '|',
    [0x2C] = 'Z',
    [0x2D] = 'X',
    [0x2E] = 'C',
    [0x2F] = 'V',
    [0x30] = 'B',
    [0x31] = 'N',
    [0x32] = 'M',
    [0x33] = '<',
    [0x34] = '>',
    [0x35] = '?',
};

// Extended scancode (E0 prefix) to ASCII/Keycode conversion
static const char scancode_e0_to_ascii[128] = {
    [0x1C] = '\n', // Keypad Enter
    [0x35] = '/',  // Keypad /
    // Most extended keys don't have direct ASCII representation (arrows, etc.)
};

/**
 * Convert scancode to ASCII character
 */
char keyboard_scancode_to_ascii(uint8_t scancode, bool shift, bool caps_lock) {
    if (scancode >= 128) {
        return 0;
    }
    
    // Handle extended keys if prefix was E0
    if (keyboard_state.extended_scancode) {
        if (scancode_e0_to_ascii[scancode]) {
            return scancode_e0_to_ascii[scancode];
        }
        return 0;
    }
    
    bool uppercase = (shift || caps_lock) && !(shift && caps_lock);
    
    if (uppercase && scancode_to_ascii_shift[scancode]) {
        return scancode_to_ascii_shift[scancode];
    }
    
    char c = scancode_to_ascii_normal[scancode];
    if (c && uppercase && c >= 'a' && c <= 'z') {
        return c - 32;  // Convert to uppercase
    }
    
    return c;
}

/**
 * Handle keyboard interrupt
 */
void keyboard_interrupt_handler(void) {
    if (!keyboard_state.initialized) {
        return;
    }
    
    // Read scancode
    uint8_t scancode = ps2_read_data();
    
    // Handle extended scancodes (0xE0 prefix)
    if (scancode == 0xE0) {
        keyboard_state.extended_scancode = true;
        return;
    }
    
    // Handle break codes (key release) - prefixed with 0xF0
    if (scancode == 0xF0) {
        keyboard_state.last_scancode = 0xF0;
        return;
    }
    
    // Determine if this is a key press or release
    bool is_release = false;
    if (keyboard_state.last_scancode == 0xF0) {
        is_release = true;
        keyboard_state.last_scancode = 0;
    }
    
    // Clear extended flag if set
    if (keyboard_state.extended_scancode) {
        keyboard_state.extended_scancode = false;
        // Extended keys might need special handling
    }
    
    // Update modifier keys
    switch (scancode) {
        case 0x2A: case 0x36:  // Left/Right Shift
            keyboard_state.shift_pressed = !is_release;
            break;
        case 0x1D:  // Left/Right Ctrl
            keyboard_state.ctrl_pressed = !is_release;
            break;
        case 0x38:  // Left/Right Alt
            keyboard_state.alt_pressed = !is_release;
            break;
        case 0x3A:  // Caps Lock (toggle on press)
            if (!is_release) {
                keyboard_state.caps_lock = !keyboard_state.caps_lock;
            }
            break;
        case 0x45:  // Num Lock (toggle on press)
            if (!is_release) {
                keyboard_state.num_lock = !keyboard_state.num_lock;
            }
            break;
        case 0x46:  // Scroll Lock (toggle on press)
            if (!is_release) {
                keyboard_state.scroll_lock = !keyboard_state.scroll_lock;
            }
            break;
    }
    
    // Create key event
    key_event_t event = {0};
    event.scancode = scancode;
    event.keycode = scancode;
    event.state = is_release ? KEY_STATE_RELEASED : KEY_STATE_PRESSED;
    event.shift = keyboard_state.shift_pressed;
    event.ctrl = keyboard_state.ctrl_pressed;
    event.alt = keyboard_state.alt_pressed;
    event.caps_lock = keyboard_state.caps_lock;
    
    if (!is_release) {
        event.ascii = keyboard_scancode_to_ascii(scancode, 
            keyboard_state.shift_pressed, 
            keyboard_state.caps_lock);
    }
    
    // Send to input event system
    extern void input_handle_keyboard(key_event_t*);
    input_handle_keyboard(&event);
    
    // Also call callback if set
    if (keyboard_state.callback) {
        keyboard_state.callback(&event);
    }
}

/**
 * Set keyboard callback
 */
error_code_t keyboard_set_callback(keyboard_callback_t callback) {
    keyboard_state.callback = callback;
    return ERR_OK;
}

/**
 * Initialize keyboard
 */
error_code_t keyboard_init(void) {
    kinfo("Initializing PS/2 keyboard...\n");
    
    // Initialize PS/2 controller first
    error_code_t err = ps2_init();
    if (err != ERR_OK) {
        return err;
    }
    
    // Reset keyboard
    err = ps2_send_byte(0xFF);  // Reset command
    if (err != ERR_OK) {
        kwarn("Keyboard reset failed\n");
    }
    
    // Wait for self-test response (0xAA)
    if (ps2_wait_output() == ERR_OK) {
        uint8_t response = ps2_read_data();
        if (response == PS2_RESP_SELF_TEST_OK) {
            kinfo("Keyboard self-test OK\n");
        } else {
            kwarn("Keyboard self-test failed: 0x%02x\n", response);
        }
    }
    
    // Set scancode set 1 (default)
    ps2_send_byte(0xF0);  // Set scancode set
    ps2_send_byte(0x01);  // Scancode set 1
    
    // Enable scanning
    ps2_send_byte(0xF4);  // Enable scanning
    
    keyboard_state.initialized = true;
    kinfo("PS/2 keyboard initialized\n");
    
    return ERR_OK;
}

