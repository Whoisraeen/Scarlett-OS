/**
 * @file taskbar.h
 * @brief Taskbar/Panel interface
 */

#ifndef KERNEL_DESKTOP_TASKBAR_H
#define KERNEL_DESKTOP_TASKBAR_H

#include "../types.h"
#include "../errors.h"
#include "../window/window.h"

// Taskbar item
typedef struct taskbar_item {
    window_t* window;
    char title[64];
    bool active;
    struct taskbar_item* next;
} taskbar_item_t;

// Taskbar position
typedef enum {
    TASKBAR_POSITION_BOTTOM,
    TASKBAR_POSITION_TOP,
    TASKBAR_POSITION_LEFT,
    TASKBAR_POSITION_RIGHT
} taskbar_position_t;

// Taskbar structure
typedef struct {
    uint32_t x, y;
    uint32_t width, height;
    taskbar_position_t position;
    uint32_t height_px;          // Height in pixels
    bool auto_hide;
    taskbar_item_t* items;
    void* start_button_widget;   // Start menu button widget
    bool initialized;
} taskbar_t;

// Taskbar functions
error_code_t taskbar_init(void);
error_code_t taskbar_add_window(window_t* window);
error_code_t taskbar_remove_window(window_t* window);
error_code_t taskbar_set_active_window(window_t* window);
error_code_t taskbar_render(void);
taskbar_t* taskbar_get(void);

#endif // KERNEL_DESKTOP_TASKBAR_H

