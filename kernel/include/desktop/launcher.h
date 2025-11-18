/**
 * @file launcher.h
 * @brief Application launcher/Start menu interface
 */

#ifndef KERNEL_DESKTOP_LAUNCHER_H
#define KERNEL_DESKTOP_LAUNCHER_H

#include "../types.h"
#include "../errors.h"
#include "../window/window.h"

// Application entry
typedef struct {
    char name[64];
    char icon_path[256];      // Path to icon
    char executable_path[256]; // Path to executable
    void* icon_data;          // Icon pixel data
    uint32_t icon_width;
    uint32_t icon_height;
} app_entry_t;

// Launcher window
typedef struct {
    window_t* window;
    app_entry_t* apps;
    uint32_t app_count;
    uint32_t app_capacity;
    bool visible;
    bool initialized;
} launcher_t;

// Launcher functions
error_code_t launcher_init(void);
error_code_t launcher_show(void);
error_code_t launcher_hide(void);
error_code_t launcher_toggle(void);
error_code_t launcher_add_app(const char* name, const char* icon_path, const char* executable_path);
error_code_t launcher_render(void);
error_code_t launcher_handle_click(uint32_t x, uint32_t y);
launcher_t* launcher_get(void);

#endif // KERNEL_DESKTOP_LAUNCHER_H

