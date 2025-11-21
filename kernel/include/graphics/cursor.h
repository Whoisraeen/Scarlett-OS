/**
 * @file cursor.h
 * @brief Mouse cursor rendering interface
 */

#ifndef KERNEL_GRAPHICS_CURSOR_H
#define KERNEL_GRAPHICS_CURSOR_H

#include "../types.h"

// Cursor types
typedef enum {
    CURSOR_ARROW,           // Standard arrow cursor
    CURSOR_TEXT,            // Text selection cursor (I-beam)
    CURSOR_HAND,            // Hand/pointer cursor
    CURSOR_RESIZE_H,        // Horizontal resize
    CURSOR_RESIZE_V,        // Vertical resize
    CURSOR_RESIZE_DIAG1,    // Diagonal resize (top-left to bottom-right)
    CURSOR_RESIZE_DIAG2,    // Diagonal resize (top-right to bottom-left)
    CURSOR_WAIT,            // Wait/busy cursor
    CURSOR_CROSSHAIR,       // Crosshair cursor
    CURSOR_NONE             // No cursor (hidden)
} cursor_type_t;

// Cursor structure
typedef struct {
    cursor_type_t type;
    uint32_t x, y;          // Cursor position
    bool visible;            // Is cursor visible?
    uint32_t hot_x, hot_y;  // Hotspot (point where click happens)
} cursor_t;

// Initialize cursor system
void cursor_init(void);

// Set cursor type
void cursor_set_type(cursor_type_t type);

// Get current cursor type
cursor_type_t cursor_get_type(void);

// Set cursor position
void cursor_set_position(uint32_t x, uint32_t y);

// Get cursor position
void cursor_get_position(uint32_t* x, uint32_t* y);

// Show/hide cursor
void cursor_show(void);
void cursor_hide(void);
bool cursor_is_visible(void);

// Render cursor at current position
void cursor_render(void);

// Render cursor at specific position
void cursor_render_at(uint32_t x, uint32_t y);

// Get cursor dimensions
void cursor_get_size(uint32_t* width, uint32_t* height);

// Get cursor hotspot
void cursor_get_hotspot(uint32_t* hot_x, uint32_t* hot_y);

#endif // KERNEL_GRAPHICS_CURSOR_H

