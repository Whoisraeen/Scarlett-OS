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
    extern window_t* window_create(int32_t x, int32_t y, uint32_t width, uint32_t height, const char* title);
    launcher_state.window = window_create(100, 100, 400, 600, "Applications");
    if (!launcher_state.window) {
        kfree(launcher_state.apps);
        return ERR_OUT_OF_MEMORY;
    }
    
    // Hidden by default
    extern error_code_t window_set_visible(window_t* window, bool visible);
    window_set_visible(launcher_state.window, false);
    
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
    if (launcher_state.window) {
        extern error_code_t window_set_visible(window_t* window, bool visible);
        window_set_visible(launcher_state.window, true);
    }
    
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
        extern error_code_t window_set_visible(window_t* window, bool visible);
        window_set_visible(launcher_state.window, false);
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
 * Render launcher with modern glassmorphism effect
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

    uint32_t launcher_radius = 20;  // Heavily rounded corners for modern look

    // Draw shadow for depth
    gfx_draw_shadow(win->x, win->y, win->width, win->height, launcher_radius, 40);

    // Window background with frosted glass effect
    gfx_fill_rounded_rect_alpha(win->x, win->y, win->width, win->height,
                                launcher_radius, RGB(35, 42, 60), 235);

    // Window border with subtle glow
    gfx_draw_rounded_rect(win->x, win->y, win->width, win->height,
                         launcher_radius, RGBA(255, 255, 255, 70));

    // Title bar with glassmorphism
    uint32_t title_bar_h = 48;
    gfx_fill_rounded_rect_alpha(win->x, win->y, win->width, title_bar_h,
                                launcher_radius, RGB(50, 58, 78), 250);

    // Title text
    gfx_draw_string(win->x + 20, win->y + 18, win->title,
                   RGB(255, 255, 255), 0);

    // Subtle separator line below title
    gfx_draw_line(win->x + 16, win->y + title_bar_h - 1,
                 win->x + win->width - 16, win->y + title_bar_h - 1,
                 RGBA(255, 255, 255, 30));

    // Modern app grid with glassmorphism cards
    uint32_t app_x = win->x + 20;
    uint32_t app_y = win->y + title_bar_h + 20;
    uint32_t app_item_w = 100;
    uint32_t app_item_h = 100;
    uint32_t card_radius = 16;  // Rounded corners for app cards
    uint32_t spacing = 16;
    uint32_t apps_per_row = (win->width - 40) / (app_item_w + spacing);

    for (uint32_t i = 0; i < launcher_state.app_count; i++) {
        app_entry_t* app = &launcher_state.apps[i];
        uint32_t row = i / apps_per_row;
        uint32_t col = i % apps_per_row;

        uint32_t item_x = app_x + col * (app_item_w + spacing);
        uint32_t item_y = app_y + row * (app_item_h + spacing);

        // Modern glass card with hover effect
        // Draw card shadow for depth
        gfx_draw_shadow(item_x, item_y, app_item_w, app_item_h, card_radius, 12);

        // Card background with frosted glass
        gfx_fill_rounded_rect_alpha(item_x, item_y, app_item_w, app_item_h,
                                    card_radius, RGB(55, 65, 88), 200);

        // Card border with subtle highlight
        gfx_draw_rounded_rect(item_x, item_y, app_item_w, app_item_h,
                             card_radius, RGBA(255, 255, 255, 50));

        // Icon area with gradient background
        uint32_t icon_area_h = 60;
        gfx_fill_gradient_rect(item_x + 2, item_y + 2, app_item_w - 4, icon_area_h,
                              RGB(70, 85, 115), RGB(55, 70, 100), true);

        // App icon (modern placeholder)
        // Draw a colorful rounded square as icon
        uint32_t icon_size = 32;
        uint32_t icon_x = item_x + (app_item_w - icon_size) / 2;
        uint32_t icon_y = item_y + 15;

        // Colorful icon background (different color per app)
        uint32_t icon_colors[] = {
            RGB(100, 150, 255),  // Blue
            RGB(150, 100, 255),  // Purple
            RGB(255, 100, 150),  // Pink
            RGB(100, 200, 150),  // Teal
            RGB(255, 180, 80),   // Orange
        };
        uint32_t icon_color = icon_colors[i % 5];

        gfx_fill_rounded_rect(icon_x, icon_y, icon_size, icon_size,
                             8, icon_color);

        // Icon symbol (simple representation)
        gfx_draw_string(icon_x + 12, icon_y + 12, "*",
                       RGB(255, 255, 255), 0);

        // App name with proper centering
        uint32_t name_y = item_y + icon_area_h + 12;
        uint32_t name_len = strlen(app->name);
        uint32_t name_x = item_x + (app_item_w - name_len * 8) / 2;

        // Ensure name fits (truncate if needed)
        char display_name[13];
        if (name_len > 12) {
            strncpy(display_name, app->name, 9);
            display_name[9] = '.';
            display_name[10] = '.';
            display_name[11] = '.';
            display_name[12] = '\0';
            name_x = item_x + 6;  // Adjust for truncated name
        } else {
            strncpy(display_name, app->name, sizeof(display_name) - 1);
            display_name[sizeof(display_name) - 1] = '\0';
        }

        gfx_draw_string(name_x, name_y, display_name,
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
    if ((int32_t)x < win->x || (int32_t)x > win->x + (int32_t)win->width ||
        (int32_t)y < win->y || (int32_t)y > win->y + (int32_t)win->height) {
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

