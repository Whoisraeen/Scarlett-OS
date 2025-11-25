/**
 * @file launcher.c
 * @brief Application Launcher Implementation
 */

#include "launcher.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../libs/libc/include/syscall.h" // For syscall wrappers
#include "../../libs/libgui/include/compositor_ipc.h" // For compositor_get_screen_info

// Global launcher context for callbacks
static launcher_ctx_t* g_launcher_ctx = NULL;

// Syscall wrappers (copied from desktop/editor for consistency)
static int sys_open(const char* path, int flags) {
    return (int)syscall(SYS_OPEN, (uint64_t)path, (uint64_t)flags, 0, 0, 0);
}

static int sys_close(int fd) {
    return (int)syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
}

static long sys_read(int fd, void* buf, size_t count) {
    return (long)syscall(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count, 0, 0);
}

static int sys_fork(void) {
    return (int)syscall(SYS_FORK, 0, 0, 0, 0, 0);
}

static int sys_exec(const char* path, char* const argv[], char* const envp[]) {
    return (int)syscall(SYS_EXEC, (uint64_t)path, (uint64_t)argv, (uint64_t)envp, 0, 0);
}

static uint64_t sys_get_uptime_ms(void) {
    return syscall(SYS_GET_UPTIME_MS, 0, 0, 0, 0, 0);
}

static void sys_yield(void) {
    syscall(SYS_YIELD, 0, 0, 0, 0, 0);
}

// O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC are defined in syscall.h (via unistd.h or fcntl.h) or should be.
// For now, assume simple int flags.
#ifndef O_RDONLY
#define O_RDONLY 0
#endif
#ifndef O_WRONLY
#define O_WRONLY 1
#endif
#ifndef O_CREAT
#define O_CREAT 0x40 // Assuming standard-ish
#endif
#ifndef O_TRUNC
#define O_TRUNC 0x200
#endif

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

// Helper: Generate a simple icon (e.g., colored square)
static uint32_t* generate_icon_pixels(uint32_t width, uint32_t height, uint32_t color) {
    uint32_t* pixels = (uint32_t*)malloc(width * height * sizeof(uint32_t));
    if (!pixels) return NULL;
    for (uint32_t i = 0; i < width * height; i++) {
        pixels[i] = color;
    }
    return pixels;
}

// Create launcher
launcher_ctx_t* launcher_create(compositor_ctx_t* compositor) {
    launcher_ctx_t* ctx = (launcher_ctx_t*)malloc(sizeof(launcher_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(launcher_ctx_t));
    g_launcher_ctx = ctx; // Set global context

    ctx->compositor = compositor;
    ctx->current_category = CAT_ALL;
    ctx->grid_columns = 6;
    ctx->grid_rows = 4;

    // Query screen dimensions from compositor
    uint32_t screen_width = 1920; // Default if compositor not ready
    uint32_t screen_height = 1080;
    compositor_get_screen_info(&screen_width, &screen_height);

    // Create launcher window (centered, 80% of screen)
    uint32_t width = (screen_width * 80) / 100;
    uint32_t height = (screen_height * 80) / 100;
    int32_t x = (screen_width - width) / 2;
    int32_t y = (screen_height - height) / 2;

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
        if (ctx->applications[i].icon_pixels) {
            free(ctx->applications[i].icon_pixels);
        }
    }

    if (ctx->launcher_window) {
        window_destroy(ctx->launcher_window);
    }

    free(ctx);
}

// Helper to parse .desktop file
static void parse_desktop_file(const char* file_path, app_entry_t* app) {
    int fd = sys_open(file_path, O_RDONLY);
    if (fd < 0) return;

    char buffer[1024];
    long bytes_read;
    char line[256];
    uint32_t line_idx = 0;

    while ((bytes_read = sys_read(fd, buffer, sizeof(buffer))) > 0) {
        for (long i = 0; i < bytes_read; i++) {
            if (buffer[i] == '\n') {
                line[line_idx] = '\0';
                // Parse line
                if (strncmp(line, "Name=", 5) == 0) {
                    strncpy(app->name, line + 5, sizeof(app->name) - 1);
                    app->name[sizeof(app->name) - 1] = '\0';
                } else if (strncmp(line, "Exec=", 5) == 0) {
                    strncpy(app->executable, line + 5, sizeof(app->executable) - 1);
                    app->executable[sizeof(app->executable) - 1] = '\0';
                } else if (strncmp(line, "Icon=", 5) == 0) {
                    strncpy(app->icon_path, line + 5, sizeof(app->icon_path) - 1);
                    app->icon_path[sizeof(app->icon_path) - 1] = '\0';
                } else if (strncmp(line, "Categories=", 11) == 0) {
                    // Simple category parsing: take first category
                    char* cat_start = line + 11;
                    char* cat_end = strchr(cat_start, ';');
                    if (cat_end) *cat_end = '\0';
                    if (strcmp(cat_start, "System") == 0) app->category = CAT_SYSTEM;
                    else if (strcmp(cat_start, "Utility") == 0) app->category = CAT_UTILITIES;
                    else if (strcmp(cat_start, "Development") == 0) app->category = CAT_DEVELOPMENT;
                    // ... add more mappings as needed
                }
                line_idx = 0;
            } else if (line_idx < sizeof(line) - 1) {
                line[line_idx++] = buffer[i];
            }
        }
    }
    sys_close(fd);
}

// Load applications from directory
void launcher_load_applications(launcher_ctx_t* ctx, const char* apps_dir) {
    if (!ctx || !apps_dir) return;

    // Use sys_open and sys_read on directory to get entries (similar to file manager)
    int fd = sys_open(apps_dir, O_RDONLY);
    if (fd < 0) {
        // Fallback to dummy applications if directory can't be opened
        launcher_add_application(ctx, "File Manager", "/usr/bin/filemanager", "filemanager", CAT_SYSTEM);
        launcher_add_application(ctx, "Terminal", "/usr/bin/terminal", "terminal", CAT_SYSTEM);
        launcher_add_application(ctx, "Text Editor", "/usr/bin/editor", "editor", CAT_ACCESSORIES);
        launcher_add_application(ctx, "Settings", "/usr/bin/settings", "settings", CAT_SETTINGS);
        return;
    }

    char dir_buffer[4096];
    long bytes_read;
    
    while ((bytes_read = sys_read(fd, dir_buffer, sizeof(dir_buffer))) > 0) {
        // Assuming sfs_dirent_t structure for directory entries
        int entries_in_block = bytes_read / sizeof(sfs_dirent_t);
        sfs_dirent_t* dirents = (sfs_dirent_t*)dir_buffer;

        for (int i = 0; i < entries_in_block; i++) {
            if (dirents[i].inode == 0) continue;
            
            // Check for .desktop extension
            const char* ext = strrchr(dirents[i].name, '.');
            if (ext && strcmp(ext, ".desktop") == 0) {
                char app_path[512];
                snprintf(app_path, 511, "%s/%s", apps_dir, dirents[i].name);

                app_entry_t new_app = {0};
                new_app.id = ctx->app_count + 1; // Assign temp ID
                new_app.category = CAT_UTILITIES; // Default category
                parse_desktop_file(app_path, &new_app);
                
                // Add to launcher applications
                if (ctx->app_count < MAX_APPLICATIONS) {
                    ctx->applications[ctx->app_count] = new_app;
                    ctx->applications[ctx->app_count].id = ctx->app_count + 1; // Final ID
                    ctx->app_count++;
                }
            }
        }
    }
    sys_close(fd);

    launcher_update_grid(ctx);
}

// Add application
void launcher_add_application(launcher_ctx_t* ctx, const char* name, const char* exec, const char* icon_name, app_category_t category) {
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

    if (icon_name) {
        strncpy(app->icon_path, icon_name, 255); // Store just the icon name
        app->icon_path[255] = '\0';
        // Generate a simple square icon (e.g., 64x64 pixels)
        app->icon_width = 64;
        app->icon_height = 64;
        // Simple color based on name hash for uniqueness
        uint32_t color = 0xFF000000 | (app->name[0] * 10) << 16 | (app->name[1] * 5) << 8 | (app->name[2] * 2);
        app->icon_pixels = generate_icon_pixels(app->icon_width, app->icon_height, color);
    } else {
        app->icon_pixels = NULL;
        app->icon_width = 0;
        app->icon_height = 0;
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
            if (ctx->applications[i].icon_pixels) {
                free(ctx->applications[i].icon_pixels);
            }

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

            int pid = sys_fork();
            if (pid == 0) {
                // Child process
                char* argv[] = {(char*)app->executable, NULL};
                char* envp[] = {NULL};
                sys_exec(app->executable, argv, envp);
                // If exec returns, it failed
                exit(1);
            }
            
            // Update launch statistics
            app->launch_count++;
            app->last_launch_time = sys_get_uptime_ms();

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
    if (g_launcher_ctx) {
        launcher_set_category(g_launcher_ctx, (app_category_t)cat_idx);
    }
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
    g_launcher_ctx = ctx; // Ensure global context is set for callbacks

    // Load applications
    launcher_load_applications(ctx, "/usr/share/applications");
    
    // Create and register IPC port
    uint64_t launcher_port_id = sys_ipc_create_port();
    if (launcher_port_id == 0) {
        printf("Failed to create launcher IPC port\n");
        return;
    }
    sys_set_process_ipc_port(launcher_port_id);
    
    printf("Launcher running on port %lu...\n", launcher_port_id);

    while (ctx->running) {
        // Process IPC messages
        // For now, only render if visible
        if (ctx->visible) {
            launcher_render(ctx);
        }
        sys_yield();
    }
}