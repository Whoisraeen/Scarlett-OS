/**
 * @file mouse.h
 * @brief PS/2 mouse driver interface
 */

#ifndef KERNEL_DRIVERS_MOUSE_H
#define KERNEL_DRIVERS_MOUSE_H

#include "../types.h"
#include "../errors.h"

// Mouse buttons
#define MOUSE_BUTTON_LEFT    0x01
#define MOUSE_BUTTON_RIGHT   0x02
#define MOUSE_BUTTON_MIDDLE  0x04
#define MOUSE_BUTTON_4       0x08
#define MOUSE_BUTTON_5       0x10

// Mouse event structure
typedef struct {
    int32_t x;          // Relative X movement
    int32_t y;          // Relative Y movement
    int32_t scroll;     // Scroll wheel movement
    uint8_t buttons;    // Button states
    bool button_left;
    bool button_right;
    bool button_middle;
} mouse_event_t;

// Mouse callback function type
typedef void (*mouse_callback_t)(mouse_event_t* event);

// Mouse functions
error_code_t mouse_init(void);
error_code_t mouse_set_callback(mouse_callback_t callback);
void mouse_interrupt_handler(void);

#endif // KERNEL_DRIVERS_MOUSE_H

