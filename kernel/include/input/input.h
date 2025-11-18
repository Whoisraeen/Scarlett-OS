/**
 * @file input.h
 * @brief Input event system interface
 */

#ifndef KERNEL_INPUT_INPUT_H
#define KERNEL_INPUT_INPUT_H

#include "../types.h"
#include "../errors.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"

// Input event types
typedef enum {
    INPUT_EVENT_KEYBOARD,
    INPUT_EVENT_MOUSE,
    INPUT_EVENT_NONE
} input_event_type_t;

// Unified input event structure
typedef struct {
    input_event_type_t type;
    union {
        key_event_t keyboard;
        mouse_event_t mouse;
    } data;
} input_event_t;

// Input event queue functions
error_code_t input_event_init(void);
error_code_t input_event_enqueue(input_event_t* event);
error_code_t input_event_dequeue(input_event_t* event);
bool input_event_available(void);
void input_event_clear(void);

// Event handlers (called by drivers)
void input_handle_keyboard(key_event_t* event);
void input_handle_mouse(mouse_event_t* event);

#endif // KERNEL_INPUT_INPUT_H

