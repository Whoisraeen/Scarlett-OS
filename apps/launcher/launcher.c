/**
 * @file launcher.c
 * @brief Application Launcher Implementation
 */

#include "launcher.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Category names
static const char* category_names[] = {
    "All",
    "Accessories",
    "Development",
    "Education",
    "Games",
    "Graphics",
    "Internet",
    "Multimedia",
    "Office",
    "Science",
    "Settings",
    "System",
    "Utilities",
};

// Create launcher
launcher_ctx_t* launcher_create(compositor_ctx_t* compositor) {
    launcher_ctx_t* ctx = (launcher_ctx_t*)malloc(sizeof(launcher_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(launcher_ctx_t));

    ctx->compositor = compositor;
    ctx->current_category = CAT_ALL;
    ctx->grid_columns = 6;
    ctx->grid_rows = 4;

    // Create launcher window (centered, 80% of screen)
    uint32_t width = (compositor->screen_width * 80) / 100;
    uint32_t height = (compositor->screen_height * 80) / 100;
    int32_t x = (compositor->screen_width - width) / 2;
    int32_t y = (compositor->screen_height - height) / 2;

    ctx->launcher_window = window_create("Applications", width, height);
    if (!ctx->launcher_window) {
        free(ctx);
        return NULL;
    }

    // Create root panel
    widget_t* root = panel_create();
    widget_set_size(root, width, height);
    widget_set_colors(root, 0xFF000000, 0xFFECF0F1);  // Black text, light gray background
    ctx->launcher_window->root = root;

    // Create search input
    ctx->search_input = text_input_create();
    widget_set_position(ctx->search_input, 20, 20);
    widget_set_size(ctx->search_input, width - 40, 40);
    text_input_set_placeholder(ctx->search_input, "Search applications...");
    widget_add_child(root, ctx->search_input);

    // Create category panel
    ctx->category_panel = panel_create();
    widget_set_position(ctx->category_panel, 20, 80);
    widget_set_size(ctx->category_panel, 150, height - 120);
    widget_set_colors(ctx->category_panel, 0xFF000000, 0xFFBDC3C7);
    widget_add_child(root, ctx->category_panel);

    // Create category buttons
    for (uint32_t i = 0; i < 13; i++) {
        category_filter_t* cat = &ctx->categories[i];
        cat->name = category_names[i];
        cat->category = (app_category_t)i;

        cat->button = button_create(cat->name);
        widget_set_position(cat->button, 5, 5 + (i * 40));
        widget_set_size(cat->button, 140, 35);
        widget_set_click_handler(cat->button, launcher_category_clicked, (void*)(uintptr_t)i);
        widget_add_child(ctx->category_panel, cat->button);
    }
    ctx->category_count = 13;

    // Create application grid panel
    ctx->app_grid_panel = panel_create();
    widget_set_position(ctx->app_grid_panel, 190, 80);
    widget_set_size(ctx->app_grid_panel, width - 230, height - 120);
    widget_set_colors(ctx->app_grid_panel, 0xFF000000, 0xFFFFFFFF);
    widget_add_child(root, ctx->app_grid_panel);

    // Create recent apps panel
    ctx->recent_panel = panel_create();
    widget_set_position(ctx->recent_panel, width - 180, 80);
    widget_set_size(ctx->recent_panel, 160, 300);
    widget_set_colors(ctx->recent_panel, 0xFF000000, 0xFFE8ECEF);
    widget_add_child(root, ctx->recent_panel);

    widget_t* recent_label = label_create("Recent");
    widget_set_position(recent_label, 10, 10);
    widget_add_child(ctx->recent_panel, recent_label);

    ctx->visible = false;
    ctx->running = true;

    return ctx;
}

// Destroy launcher
void launcher_destroy(launcher_ctx_t* ctx) {
    if (!ctx) return;

    // Free application icons
    for (uint32_t i = 0; i < ctx->app_count; i++) {
        // TODO: Free icon data
    }

    if (ctx->launcher_window) {
        window_destroy(ctx->launcher_window);
    }

    free(ctx);
}

// Load applications from directory
void launcher_load_applications(launcher_ctx_t* ctx, const char* apps_dir) {
    if (!ctx || !apps_dir) return;

    // TODO: Scan /usr/share/applications for .desktop files
    // For now, add some default applications

    launcher_add_application(ctx, "File Manager", "/usr/bin/filemanager", "/usr/share/icons/filemanager.png", CAT_SYSTEM);
    launcher_add_application(ctx, "Terminal", "/usr/bin/terminal", "/usr/share/icons/terminal.png", CAT_SYSTEM);
    launcher_add_application(ctx, "Text Editor", "/usr/bin/editor", "/usr/share/icons/editor.png", CAT_ACCESSORIES);
    launcher_add_application(ctx, "Settings", "/usr/bin/settings", "/usr/share/icons/settings.png", CAT_SETTINGS);
    launcher_add_application(ctx, "Web Browser", "/usr/bin/browser", "/usr/share/icons/browser.png", CAT_INTERNET);
    launcher_add_application(ctx, "Calculator", "/usr/bin/calculator", "/usr/share/icons/calculator.png", CAT_UTILITIES);
    launcher_add_application(ctx, "Image Viewer", "/usr/bin/imageviewer", "/usr/share/icons/imageviewer.png", CAT_GRAPHICS);
    launcher_add_application(ctx, "Music Player", "/usr/bin/musicplayer", "/usr/share/icons/music.png", CAT_MULTIMEDIA);
}

// Add application
void launcher_add_application(launcher_ctx_t* ctx, const char* name, const char* exec, const char* icon, app_category_t category) {
    if (!ctx || !name || !exec || ctx->app_count >= MAX_APPLICATIONS) {
        return;
    }

    app_entry_t* app = &ctx->applications[ctx->app_count];
    app->id = ctx->app_count + 1;
    app->category = category;
    app->favorite = false;
    app->launch_count = 0;
    app->last_launch_time = 0;

    strncpy(app->name, name, 63);
    app->name[63] = '\0';

    strncpy(app->executable, exec, 255);
    app->executable[255] = '\0';

    if (icon) {
        strncpy(app->icon_path, icon, 255);
        app->icon_path[255] = '\0';
        // TODO: Load icon image
    }

    ctx->app_count++;

    // Update grid display
    launcher_update_grid(ctx);
}

// Remove application
void launcher_remove_application(launcher_ctx_t* ctx, uint32_t app_id) {
    if (!ctx || app_id == 0) return;

    for (uint32_t i = 0; i < ctx->app_count; i++) {
        if (ctx->applications[i].id == app_id) {
            // TODO: Free icon

            memmove(&ctx->applications[i],
                    &ctx->applications[i + 1],
                    (ctx->app_count - i - 1) * sizeof(app_entry_t));
            ctx->app_count--;

            launcher_update_grid(ctx);
            break;
        }
    }
}

// Find application by name
app_entry_t* launcher_find_application(launcher_ctx_t* ctx, const char* name) {
    if (!ctx || !name) return NULL;

    for (uint32_t i = 0; i < ctx->app_count; i++) {
        if (strcmp(ctx->applications[i].name, name) == 0) {
            return &ctx->applications[i];
        }
    }

    return NULL;
}

// Launch application
void launcher_launch_application(launcher_ctx_t* ctx, uint32_t app_id) {
    if (!ctx || app_id == 0) return;

    for (uint32_t i = 0; i < ctx->app_count; i++) {
        if (ctx->applications[i].id == app_id) {
            app_entry_t* app = &ctx->applications[i];

            // TODO: Launch application via exec syscall
            printf("Launching: %s (%s)\n", app->name, app->executable);

            // Update launch statistics
            app->launch_count++;
            // TODO: Get current time
            app->last_launch_time = 0;

            // Update recent apps
            launcher_update_recent(ctx, app_id);

            // Hide launcher after launch
            launcher_hide(ctx);

            break;
        }
    }
}

// Add to favorites
void launcher_add_to_favorites(launcher_ctx_t* ctx, uint32_t app_id) {
    if (!ctx || app_id == 0 || ctx->favorite_count >= MAX_FAVORITE_APPS) {
        return;
    }

    for (uint32_t i = 0; i < ctx->app_count; i++) {
        if (ctx->applications[i].id == app_id) {
            ctx->applications[i].favorite = true;
            ctx->favorites[ctx->favorite_count++] = &ctx->applications[i];
            break;
        }
    }
}

// Remove from favorites
void launcher_remove_from_favorites(launcher_ctx_t* ctx, uint32_t app_id) {
    if (!ctx || app_id == 0) return;

    for (uint32_t i = 0; i < ctx->favorite_count; i++) {
        if (ctx->favorites[i]->id == app_id) {
            ctx->favorites[i]->favorite = false;

            memmove(&ctx->favorites[i],
                    &ctx->favorites[i + 1],
                    (ctx->favorite_count - i - 1) * sizeof(app_entry_t*));
            ctx->favorite_count--;
            break;
        }
    }
}

// Update recent apps
void launcher_update_recent(launcher_ctx_t* ctx, uint32_t app_id) {
    if (!ctx || app_id == 0) return;

    app_entry_t* app = NULL;
    for (uint32_t i = 0; i < ctx->app_count; i++) {
        if (ctx->applications[i].id == app_id) {
            app = &ctx->applications[i];
            break;
        }
    }

    if (!app) return;

    // Remove from recent if already there
    for (uint32_t i = 0; i < ctx->recent_count; i++) {
        if (ctx->recent_apps[i]->id == app_id) {
            memmove(&ctx->recent_apps[i],
                    &ctx->recent_apps[i + 1],
                    (ctx->recent_count - i - 1) * sizeof(app_entry_t*));
            ctx->recent_count--;
            break;
        }
    }

    // Add to front of recent list
    if (ctx->recent_count >= MAX_RECENT_APPS) {
        // Remove oldest
        ctx->recent_count--;
    }

    memmove(&ctx->recent_apps[1],
            &ctx->recent_apps[0],
            ctx->recent_count * sizeof(app_entry_t*));
    ctx->recent_apps[0] = app;
    ctx->recent_count++;
}

// Set category filter
void launcher_set_category(launcher_ctx_t* ctx, app_category_t category) {
    if (!ctx) return;

    ctx->current_category = category;
    launcher_update_grid(ctx);

    // Update category button appearances
    for (uint32_t i = 0; i < ctx->category_count; i++) {
        if (ctx->categories[i].category == category) {
            widget_set_colors(ctx->categories[i].button, 0xFFFFFFFF, 0xFF3498DB);  // Highlight selected
        } else {
            widget_set_colors(ctx->categories[i].button, 0xFF000000, 0xFFBDC3C7);  // Normal
        }
    }
}

// Category button clicked
void launcher_category_clicked(widget_t* widget, void* userdata) {
    uint32_t cat_idx = (uint32_t)(uintptr_t)userdata;
    launcher_ctx_t* ctx = NULL;  // TODO: Get context from widget

    if (!ctx) return;

    launcher_set_category(ctx, (app_category_t)cat_idx);
}

// Search applications
void launcher_search(launcher_ctx_t* ctx, const char* query) {
    if (!ctx || !query) return;

    strncpy(ctx->search_query, query, 255);
    ctx->search_query[255] = '\0';

    launcher_update_grid(ctx);
}

// Clear search
void launcher_clear_search(launcher_ctx_t* ctx) {
    if (!ctx) return;

    ctx->search_query[0] = '\0';
    launcher_update_grid(ctx);
}

// Update application grid
void launcher_update_grid(launcher_ctx_t* ctx) {
    if (!ctx) return;

    // Clear existing app buttons
    for (uint32_t i = 0; i < MAX_APPLICATIONS; i++) {
        if (ctx->app_buttons[i]) {
            widget_remove_child(ctx->app_grid_panel, ctx->app_buttons[i]);
            widget_destroy(ctx->app_buttons[i]);
            ctx->app_buttons[i] = NULL;
        }
    }

    // Filter and display applications
    uint32_t displayed = 0;
    for (uint32_t i = 0; i < ctx->app_count; i++) {
        app_entry_t* app = &ctx->applications[i];

        // Category filter
        if (ctx->current_category != CAT_ALL && app->category != ctx->current_category) {
            continue;
        }

        // Search filter
        if (ctx->search_query[0] != '\0') {
            // Simple case-insensitive substring search
            // TODO: Implement proper search
            if (strstr(app->name, ctx->search_query) == NULL) {
                continue;
            }
        }

        // Create button for this app
        widget_t* btn = button_create(app->name);

        // Calculate grid position
        uint32_t col = displayed % ctx->grid_columns;
        uint32_t row = displayed / ctx->grid_columns;

        int32_t x = 10 + (col * 120);
        int32_t y = 10 + (row * 120);

        widget_set_position(btn, x, y);
        widget_set_size(btn, 110, 110);
        widget_set_click_handler(btn, (event_callback_t)launcher_launch_application, (void*)(uintptr_t)app->id);

        widget_add_child(ctx->app_grid_panel, btn);
        ctx->app_buttons[displayed] = btn;

        displayed++;
    }
}

// Show launcher
void launcher_show(launcher_ctx_t* ctx) {
    if (!ctx) return;

    ctx->visible = true;
    window_show(ctx->launcher_window);

    // Update grid
    launcher_update_grid(ctx);
}

// Hide launcher
void launcher_hide(launcher_ctx_t* ctx) {
    if (!ctx) return;

    ctx->visible = false;
    window_hide(ctx->launcher_window);
}

// Handle keyboard input
void launcher_handle_key(launcher_ctx_t* ctx, uint32_t keycode, bool pressed) {
    if (!ctx || !pressed) return;

    // ESC to close
    if (keycode == 27) {  // ESC key
        launcher_hide(ctx);
    }
}

// Render launcher
void launcher_render(launcher_ctx_t* ctx) {
    if (!ctx || !ctx->visible) return;

    window_render(ctx->launcher_window);
}

// Main launcher loop
void launcher_run(launcher_ctx_t* ctx) {
    if (!ctx) return;

    // Load applications
    launcher_load_applications(ctx, "/usr/share/applications");

    while (ctx->running) {
        // TODO: Process IPC messages

        if (ctx->visible) {
            launcher_render(ctx);
        }

        // TODO: Sleep or yield CPU
    }
}
