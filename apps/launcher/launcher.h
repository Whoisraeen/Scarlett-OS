/**
 * @file launcher.h
 * @brief Application Launcher for Scarlett OS
 *
 * Provides grid view of applications with search and categories
 */

#ifndef APPS_LAUNCHER_H
#define APPS_LAUNCHER_H

#include <stdint.h>
#include <stdbool.h>
#include "../../gui/compositor/src/compositor.h"
#include "../../gui/widgets/src/widgets.h"

// Launcher configuration
#define MAX_APPLICATIONS 256
#define MAX_CATEGORIES 16
#define MAX_RECENT_APPS 10
#define MAX_FAVORITE_APPS 16

// Application category
typedef enum {
    CAT_ALL = 0,
    CAT_ACCESSORIES,
    CAT_DEVELOPMENT,
    CAT_EDUCATION,
    CAT_GAMES,
    CAT_GRAPHICS,
    CAT_INTERNET,
    CAT_MULTIMEDIA,
    CAT_OFFICE,
    CAT_SCIENCE,
    CAT_SETTINGS,
    CAT_SYSTEM,
    CAT_UTILITIES,
} app_category_t;

// Application entry
typedef struct {
    uint32_t id;
    char name[64];
    char description[128];
    char executable[256];
    char icon_path[256];
    uint32_t* icon_pixels; // Pointer to pixel data
    uint32_t icon_width;
    uint32_t icon_height;
    app_category_t category;
    bool favorite;
    uint32_t launch_count;
    uint64_t last_launch_time;
} app_entry_t;

// Category filter
typedef struct {
    const char* name;
    app_category_t category;
    widget_t* button;
} category_filter_t;

// Launcher context
typedef struct {
    compositor_ctx_t* compositor;
    window_t* launcher_window;

    app_entry_t applications[MAX_APPLICATIONS];
    uint32_t app_count;

    app_entry_t* recent_apps[MAX_RECENT_APPS];
    uint32_t recent_count;

    app_entry_t* favorites[MAX_FAVORITE_APPS];
    uint32_t favorite_count;

    category_filter_t categories[MAX_CATEGORIES];
    uint32_t category_count;

    app_category_t current_category;
    char search_query[256];

    // Widgets
    widget_t* search_input;
    widget_t* category_panel;
    widget_t* app_grid_panel;
    widget_t* recent_panel;
    widget_t* app_buttons[MAX_APPLICATIONS];

    // Display state
    uint32_t grid_columns;
    uint32_t grid_rows;
    uint32_t scroll_offset;

    bool visible;
    bool running;
} launcher_ctx_t;

// Launcher initialization
launcher_ctx_t* launcher_create(compositor_ctx_t* compositor);
void launcher_destroy(launcher_ctx_t* ctx);
void launcher_run(launcher_ctx_t* ctx);

// Application management
void launcher_load_applications(launcher_ctx_t* ctx, const char* apps_dir);
void launcher_add_application(launcher_ctx_t* ctx, const char* name, const char* exec, const char* icon, app_category_t category);
void launcher_remove_application(launcher_ctx_t* ctx, uint32_t app_id);
app_entry_t* launcher_find_application(launcher_ctx_t* ctx, const char* name);

// Application launching
void launcher_launch_application(launcher_ctx_t* ctx, uint32_t app_id);
void launcher_add_to_favorites(launcher_ctx_t* ctx, uint32_t app_id);
void launcher_remove_from_favorites(launcher_ctx_t* ctx, uint32_t app_id);

// Recent apps
void launcher_update_recent(launcher_ctx_t* ctx, uint32_t app_id);

// Category filtering
void launcher_set_category(launcher_ctx_t* ctx, app_category_t category);
void launcher_category_clicked(widget_t* widget, void* userdata);

// Search
void launcher_search(launcher_ctx_t* ctx, const char* query);
void launcher_clear_search(launcher_ctx_t* ctx);

// Display
void launcher_show(launcher_ctx_t* ctx);
void launcher_hide(launcher_ctx_t* ctx);
void launcher_update_grid(launcher_ctx_t* ctx);
void launcher_render(launcher_ctx_t* ctx);

// Input handling
void launcher_handle_key(launcher_ctx_t* ctx, uint32_t keycode, bool pressed);

#endif // APPS_LAUNCHER_H
