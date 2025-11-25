/**
 * @file widgets.h
 * @brief Native Widget Toolkit for Scarlett OS
 *
 * Provides buttons, windows, menus, text inputs, lists, and more
 */

#ifndef GUI_WIDGETS_H
#define GUI_WIDGETS_H

#include <stdint.h>
#include <stdbool.h>
#include "../../../libs/libgui/include/compositor_ipc.h" // For compositor_window_state_t

// Widget types
typedef enum {
    WIDGET_BUTTON,
    WIDGET_LABEL,
    WIDGET_TEXT_INPUT,
    WIDGET_CHECKBOX,
    WIDGET_RADIO,
    WIDGET_LIST,
    WIDGET_TREE,
    WIDGET_MENU,
    WIDGET_MENU_ITEM,
    WIDGET_PANEL,
    WIDGET_SCROLLBAR,
    WIDGET_SLIDER,
    WIDGET_PROGRESS_BAR,
    WIDGET_TAB,
    WIDGET_TABLE,
} widget_type_t;

// Widget states
typedef enum {
    WIDGET_STATE_NORMAL = 0,
    WIDGET_STATE_HOVER = 1,
    WIDGET_STATE_PRESSED = 2,
    WIDGET_STATE_DISABLED = 3,
    WIDGET_STATE_FOCUSED = 4,
} widget_state_t;

// Alignment
typedef enum {
    ALIGN_LEFT = 0,
    ALIGN_CENTER = 1,
    ALIGN_RIGHT = 2,
    ALIGN_TOP = 0,
    ALIGN_MIDDLE = 1,
    ALIGN_BOTTOM = 2,
} alignment_t;

// Forward declarations
typedef struct widget widget_t;
typedef struct window window_t;

// Event callback types
typedef void (*event_callback_t)(widget_t* widget, void* userdata);
typedef void (*paint_callback_t)(widget_t* widget, void* canvas);

// Widget base structure
struct widget {
    widget_type_t type;
    widget_t* parent;
    widget_t** children;
    uint32_t child_count;
    uint32_t child_capacity;

    int32_t x, y;
    uint32_t width, height;
    widget_state_t state;
    bool visible;
    bool enabled;

    char* text;
    uint32_t fg_color;
    uint32_t bg_color;

    event_callback_t on_click;
    event_callback_t on_hover;
    event_callback_t on_focus;
    event_callback_t on_blur;
    paint_callback_t on_paint;

    void* userdata;
};

// Window structure
struct window {
    uint32_t compositor_id;  // Window ID from compositor
    widget_t* root;          // Root widget (panel)
    char* title;
    int32_t x, y;
    uint32_t width, height;
    void* framebuffer;       // Pointer to shared memory framebuffer
    uint32_t shm_id;         // Shared memory ID
    uint32_t framebuffer_size; // Size of framebuffer in bytes
    bool visible;
};

// Widget creation
widget_t* widget_create(widget_type_t type);
void widget_destroy(widget_t* widget);
void widget_add_child(widget_t* parent, widget_t* child);
void widget_remove_child(widget_t* parent, widget_t* child);

// Widget properties
void widget_set_position(widget_t* widget, int32_t x, int32_t y);
void widget_set_size(widget_t* widget, uint32_t width, uint32_t height);
void widget_set_text(widget_t* widget, const char* text);
void widget_set_colors(widget_t* widget, uint32_t fg, uint32_t bg);
void widget_set_visible(widget_t* widget, bool visible);
void widget_set_enabled(widget_t* widget, bool enabled);

// Event handling
void widget_set_click_handler(widget_t* widget, event_callback_t callback, void* userdata);
void widget_handle_mouse_move(widget_t* widget, int32_t x, int32_t y);
void widget_handle_mouse_button(widget_t* widget, int32_t x, int32_t y, bool pressed);

// Rendering
void widget_paint(widget_t* widget, void* canvas);

// Specific widgets
widget_t* button_create(const char* text);
widget_t* label_create(const char* text);
widget_t* text_input_create();
widget_t* checkbox_create(const char* text);
widget_t* list_create();
widget_t* menu_create();
widget_t* panel_create();

// Button operations
void button_set_text(widget_t* button, const char* text);

// Label operations
void label_set_text(widget_t* label, const char* text);
void label_set_alignment(widget_t* label, alignment_t halign, alignment_t valign);

// Text input operations
void text_input_set_text(widget_t* input, const char* text);
const char* text_input_get_text(widget_t* input);
void text_input_set_placeholder(widget_t* input, const char* placeholder);

// List operations
void list_add_item(widget_t* list, const char* item);
void list_remove_item(widget_t* list, uint32_t index);
void list_clear(widget_t* list);
int32_t list_get_selected(widget_t* list);

// Menu operations
widget_t* menu_add_item(widget_t* menu, const char* text, event_callback_t callback);
void menu_add_separator(widget_t* menu);

// Window operations
window_t* window_create(const char* title, uint32_t width, uint32_t height);
void window_destroy(window_t* window);
void window_show(window_t* window);
void window_hide(window_t* window);
void window_set_title(window_t* window, const char* title);
void window_add_widget(window_t* window, widget_t* widget);
void window_render(window_t* window);

#endif // GUI_WIDGETS_H