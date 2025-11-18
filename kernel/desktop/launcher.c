/**
 * @file launcher.c
 * @brief Application launcher/Start menu implementation
 */

#include "../include/desktop/launcher.h"
#include "../include/ui/widget.h"
#include "../include/ui/theme.h"
#include "../include/graphics/graphics.h"
#include "../include/graphics/framebuffer.h"
#include "../include/window/window.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"

// Launcher state
static launcher_t launcher_state = {0};

#define DEFAULT_APP_CAPACITY 32

/**
 * Initialize launcher
 */
error_code_t launcher_init(void) {
    if (launcher_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing application launcher...\n");
    
    // Allocate app list
    launcher_state.app_capacity = DEFAULT_APP_CAPACITY;
    launcher_state.apps = (app_entry_t*)kmalloc(sizeof(app_entry_t) * launcher_state.app_capacity);
    if (!launcher_state.apps) {
        return ERR_OUT_OF_MEMORY;
    }
    
    memset(launcher_state.apps, 0, sizeof(app_entry_t) * launcher_state.app_capacity);
    launcher_state.app_count = 0;
    launcher_state.window = NULL;
    launcher_state.visible = false;
    
    // Create launcher window (will be shown when needed)
    extern window_t* window_create(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const char* title);
    launcher_state.window = window_create(100, 100, 400, 600, "Applications");
    if (!launcher_state.window) {
        kfree(launcher_state.apps);
        return ERR_OUT_OF_MEMORY;
    }
    
    launcher_state.window->visible = false;  // Hidden by default
    
    launcher_state.initialized = true;
    
    // Add default apps
    launcher_add_app("Terminal", "", "/bin/terminal");
    launcher_add_app("File Manager", "", "/bin/filemanager");
    launcher_add_app("Settings", "", "/bin/settings");
    launcher_add_app("Calculator", "", "/bin/calculator");
    
    kinfo("Application launcher initialized\n");
    return ERR_OK;
}

/**
 * Add application to launcher
 */
error_code_t launcher_add_app(const char* name, const char* icon_path, const char* executable_path) {
    if (!launcher_state.initialized || !name || !executable_path) {
        return ERR_INVALID_ARG;
    }
    
    if (launcher_state.app_count >= launcher_state.app_capacity) {
        return ERR_OUT_OF_MEMORY;
    }
    
    app_entry_t* app = &launcher_state.apps[launcher_state.app_count];
    
    strncpy(app->name, name, sizeof(app->name) - 1);
    app->name[sizeof(app->name) - 1] = '\0';
    
    if (icon_path) {
        strncpy(app->icon_path, icon_path, sizeof(app->icon_path) - 1);
        app->icon_path[sizeof(app->icon_path) - 1] = '\0';
    }
    
    strncpy(app->executable_path, executable_path, sizeof(app->executable_path) - 1);
    app->executable_path[sizeof(app->executable_path) - 1] = '\0';
    
    app->icon_data = NULL;
    app->icon_width = 0;
    app->icon_height = 0;
    
    launcher_state.app_count++;
    
    return ERR_OK;
}

/**
 * Show launcher
 */
error_code_t launcher_show(void) {
    if (!launcher_state.initialized || !launcher_state.window) {
        return ERR_INVALID_STATE;
    }
    
    launcher_state.visible = true;
    launcher_state.window->visible = true;
    
    return ERR_OK;
}

/**
 * Hide launcher
 */
error_code_t launcher_hide(void) {
    if (!launcher_state.initialized || !launcher_state.window) {
        return ERR_INVALID_STATE;
    }
    
    launcher_state.visible = false;
    if (launcher_state.window) {
        launcher_state.window->visible = false;
    }
    
    return ERR_OK;
}

/**
 * Toggle launcher visibility
 */
error_code_t launcher_toggle(void) {
    if (!launcher_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    if (launcher_state.visible) {
        return launcher_hide();
    } else {
        return launcher_show();
    }
}

/**
 * Render launcher with glassmorphism effect
 */
error_code_t launcher_render(void) {
    if (!launcher_state.initialized || !launcher_state.visible || !launcher_state.window) {
        return ERR_OK;  // Not an error if hidden
    }
    
    window_t* win = launcher_state.window;
    theme_t* theme = theme_get_current();
    
    if (!theme) {
        return ERR_INVALID_STATE;
    }
    
    // Render launcher window with glassmorphism
    // Glass effect: semi-transparent with blur
    
    // Window background (glass effect)
    gfx_draw_rect_alpha(win->x, win->y, win->width, win->height,
                       RGB(40, 40, 50), 220);  // 86% opacity
    
    // Window border (subtle, glowing)
    gfx_draw_rect(win->x, win->y, win->width, win->height,
                 RGB(100, 120, 150));
    
    // Title bar
    uint32_t title_bar_h = 32;
    gfx_draw_rect_alpha(win->x, win->y, win->width, title_bar_h,
                       RGB(60, 60, 80), 240);
    gfx_draw_string(win->x + 8, win->y + 8, win->title,
                   RGB(255, 255, 255), 0);
    
    // App grid
    uint32_t app_x = win->x + 16;
    uint32_t app_y = win->y + title_bar_h + 16;
    uint32_t app_item_w = 80;
    uint32_t app_item_h = 80;
    uint32_t apps_per_row = (win->width - 32) / (app_item_w + 8);
    
    for (uint32_t i = 0; i < launcher_state.app_count; i++) {
        app_entry_t* app = &launcher_state.apps[i];
        uint32_t row = i / apps_per_row;
        uint32_t col = i % apps_per_row;
        
        uint32_t item_x = app_x + col * (app_item_w + 8);
        uint32_t item_y = app_y + row * (app_item_h + 8);
        
        // App item background (glass card)
        gfx_draw_rect_alpha(item_x, item_y, app_item_w, app_item_h,
                           RGB(50, 50, 70), 180);
        gfx_draw_rect(item_x, item_y, app_item_w, app_item_h,
                     RGB(80, 100, 120));
        
        // App icon (placeholder - would load from icon_path)
        gfx_draw_string(item_x + 32, item_y + 20, "▣", 
                       RGB(150, 150, 255), 0);
        
        // App name
        gfx_draw_string(item_x + 4, item_y + 60, app->name,
                       RGB(255, 255, 255), 0);
    }
    
    return ERR_OK;
}

/**
 * Handle launcher click
 */
error_code_t launcher_handle_click(uint32_t x, uint32_t y) {
    if (!launcher_state.initialized || !launcher_state.visible || !launcher_state.window) {
        return ERR_INVALID_STATE;
    }
    
    window_t* win = launcher_state.window;
    
    // Check if click is within launcher window
    if (x < win->x || x > win->x + win->width ||
        y < win->y || y > win->y + win->height) {
        return ERR_NOT_FOUND;
    }
    
    // Check if click is on an app item
    uint32_t title_bar_h = 32;
    uint32_t app_x = win->x + 16;
    uint32_t app_y = win->y + title_bar_h + 16;
    uint32_t app_item_w = 80;
    uint32_t app_item_h = 80;
    uint32_t apps_per_row = (win->width - 32) / (app_item_w + 8);
    
    uint32_t rel_x = x - app_x;
    uint32_t rel_y = y - app_y;
    
    uint32_t col = rel_x / (app_item_w + 8);
    uint32_t row = rel_y / (app_item_h + 8);
    uint32_t app_index = row * apps_per_row + col;
    
    if (app_index < launcher_state.app_count) {
        app_entry_t* app = &launcher_state.apps[app_index];
        
        // Launch application (would fork/exec in full implementation)
        kinfo("Launching application: %s (%s)\n", app->name, app->executable_path);
        
        // Hide launcher after launching
        launcher_hide();
        
        return ERR_OK;
    }
    
    return ERR_NOT_FOUND;
}

/**
 * Get launcher instance
 */
launcher_t* launcher_get(void) {
    return launcher_state.initialized ? &launcher_state : NULL;
}

