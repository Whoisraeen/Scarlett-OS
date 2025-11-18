/**
 * @file keyboard.h
 * @brief PS/2 keyboard driver interface
 */

#ifndef KERNEL_DRIVERS_KEYBOARD_H
#define KERNEL_DRIVERS_KEYBOARD_H

#include "../types.h"
#include "../errors.h"

// Key states
#define KEY_STATE_PRESSED    0x01
#define KEY_STATE_RELEASED   0x02

// Key event structure
typedef struct {
    uint8_t scancode;
    uint8_t keycode;
    uint8_t state;  // PRESSED or RELEASED
    bool shift;
    bool ctrl;
    bool alt;
    bool caps_lock;
    char ascii;     // ASCII character (0 if not printable)
} key_event_t;

// Keyboard callback function type
typedef void (*keyboard_callback_t)(key_event_t* event);

// Keyboard functions
error_code_t keyboard_init(void);
error_code_t keyboard_set_callback(keyboard_callback_t callback);
void keyboard_interrupt_handler(void);
char keyboard_scancode_to_ascii(uint8_t scancode, bool shift, bool caps_lock);

#endif // KERNEL_DRIVERS_KEYBOARD_H

