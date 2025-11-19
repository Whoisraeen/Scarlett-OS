/**
 * @file window.h
 * @brief Window manager interface
 */

#ifndef KERNEL_WINDOW_WINDOW_H
#define KERNEL_WINDOW_WINDOW_H

#include "../types.h"
#include "../errors.h"
#include "../graphics/graphics.h"

// Window flags
#define WINDOW_FLAG_VISIBLE    0x01
#define WINDOW_FLAG_FOCUSED    0x02
#define WINDOW_FLAG_RESIZABLE  0x04
#define WINDOW_FLAG_MINIMIZED 0x08
#define WINDOW_FLAG_MAXIMIZED 0x10

// Window structure
typedef struct window {
    uint64_t id;                    // Unique window ID
    int32_t x, y;                   // Position
    uint32_t width, height;         // Size
    uint32_t flags;                 // Window flags
    char title[256];                // Window title
    uint32_t* buffer;               // Window buffer (for double buffering)
    uint32_t bg_color;              // Background color
    uint8_t alpha;                  // Window transparency (0-255)
    uint32_t blur_radius;           // Background blur radius
    uint32_t corner_radius;         // Window corner radius
    void* widget_root;               // Root widget for this window
    struct window* next;             // Linked list
    struct window* prev;
} window_t;

// Window manager functions
error_code_t window_manager_init(void);
window_t* window_create(int32_t x, int32_t y, uint32_t width, uint32_t height, const char* title);
error_code_t window_destroy(window_t* window);
error_code_t window_set_title(window_t* window, const char* title);
error_code_t window_set_position(window_t* window, int32_t x, int32_t y);
error_code_t window_set_size(window_t* window, uint32_t width, uint32_t height);
error_code_t window_set_visible(window_t* window, bool visible);
error_code_t window_set_focus(window_t* window, bool focused);
window_t* window_get_focused(void);
window_t* window_find_at(int32_t x, int32_t y);
error_code_t window_render(window_t* window);
error_code_t window_manager_render_all(void);
error_code_t window_manager_handle_input(void);

#endif // KERNEL_WINDOW_WINDOW_H

