/**
 * @file widget.h
 * @brief UI Widget system interface
 */

#ifndef KERNEL_UI_WIDGET_H
#define KERNEL_UI_WIDGET_H

#include "../types.h"
#include "../errors.h"
#include "../graphics/graphics.h"
#include "../window/window.h"

// Widget types
typedef enum {
    WIDGET_TYPE_BUTTON,
    WIDGET_TYPE_LABEL,
    WIDGET_TYPE_TEXTBOX,
    WIDGET_TYPE_CHECKBOX,
    WIDGET_TYPE_PANEL,
    WIDGET_TYPE_CUSTOM
} widget_type_t;

// Widget flags
#define WIDGET_FLAG_VISIBLE   0x01
#define WIDGET_FLAG_ENABLED   0x02
#define WIDGET_FLAG_FOCUSED   0x04

// Widget callback function types
typedef void (*widget_click_callback_t)(void* widget, void* user_data);
typedef void (*widget_change_callback_t)(void* widget, void* user_data);
typedef void (*widget_draw_callback_t)(void* widget);

// Widget structure
typedef struct widget {
    widget_type_t type;
    uint32_t flags;
    int32_t x, y;                    // Position relative to parent
    uint32_t width, height;          // Size
    uint32_t bg_color;               // Background color
    uint32_t fg_color;               // Foreground color
    char text[256];                  // Widget text
    void* data;                      // Widget-specific data
    void* user_data;                 // User-provided data
    
    // Callbacks
    widget_click_callback_t on_click;
    widget_change_callback_t on_change;
    widget_draw_callback_t on_draw;
    
    // Hierarchy
    struct widget* parent;
    struct widget* children;
    struct widget* next;             // Sibling linked list
    struct widget* prev;
    
    // Layout
    uint32_t margin_left;
    uint32_t margin_right;
    uint32_t margin_top;
    uint32_t margin_bottom;
    uint32_t padding_left;
    uint32_t padding_right;
    uint32_t padding_top;
    uint32_t padding_bottom;
} widget_t;

// Widget functions
widget_t* widget_create(widget_type_t type, widget_t* parent);
error_code_t widget_destroy(widget_t* widget);
error_code_t widget_set_position(widget_t* widget, int32_t x, int32_t y);
error_code_t widget_set_size(widget_t* widget, uint32_t width, uint32_t height);
error_code_t widget_set_text(widget_t* widget, const char* text);
error_code_t widget_set_colors(widget_t* widget, uint32_t bg_color, uint32_t fg_color);
error_code_t widget_set_visible(widget_t* widget, bool visible);
error_code_t widget_set_enabled(widget_t* widget, bool enabled);
error_code_t widget_add_child(widget_t* parent, widget_t* child);
error_code_t widget_remove_child(widget_t* parent, widget_t* child);
error_code_t widget_render(widget_t* widget, window_t* window);
error_code_t widget_handle_mouse(widget_t* widget, int32_t mx, int32_t my, bool clicked);
error_code_t widget_handle_keyboard(widget_t* widget, uint8_t key, char ascii);

// Specific widget creation functions
widget_t* widget_create_button(widget_t* parent, const char* text, int32_t x, int32_t y, uint32_t width, uint32_t height);
widget_t* widget_create_label(widget_t* parent, const char* text, int32_t x, int32_t y);
widget_t* widget_create_textbox(widget_t* parent, const char* placeholder, int32_t x, int32_t y, uint32_t width, uint32_t height);
widget_t* widget_create_checkbox(widget_t* parent, const char* text, int32_t x, int32_t y);
widget_t* widget_create_panel(widget_t* parent, int32_t x, int32_t y, uint32_t width, uint32_t height);

#endif // KERNEL_UI_WIDGET_H

