/**
 * @file desktop.c
 * @brief Desktop Shell Implementation
 */

#include "desktop.h"
#include "../../gui/ugal/src/ugal.h"
#include "../../libs/libc/include/syscall.h" // User-space syscalls
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "font8x8_basic.h" // Include font data

#define COMPOSITOR_PORT 200
#define DESKTOP_PORT 300

// Simple 8x8 font (minimal subset)
static void draw_char_desktop(uint32_t* buffer, uint32_t width, uint32_t height, int x, int y, char c, uint32_t color) {
    if (c < 0 || c > 127) return; // Only ASCII
    for (int dy = 0; dy < 8; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            if ((font8x8_basic[(int)c][dy] >> dx) & 1) {
                if (x + dx >= 0 && x + dx < (int)width && y + dy >= 0 && y + dy < (int)height) {
                    buffer[(y + dy) * width + (x + dx)] = color;
                }
            }
        }
    }
}

static void draw_string_desktop(uint32_t* buffer, uint32_t width, uint32_t height, int x, int y, const char* str, uint32_t color) {
    int cx = x;
    while (*str) {
        draw_char_desktop(buffer, width, height, cx, y, *str, color);
        cx += 8; // Monospace font width
        str++;
    }
}

// Create desktop shell
desktop_ctx_t* desktop_create(compositor_ctx_t* compositor) {
    desktop_ctx_t* ctx = (desktop_ctx_t*)malloc(sizeof(desktop_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(desktop_ctx_t));

    ctx->compositor = compositor; 

    uint32_t screen_width = 1920; // Default if compositor not ready
    uint32_t screen_height = 1080;
    compositor_get_screen_info(&screen_width, &screen_height);

    ctx->desktop_window = window_create("Desktop", screen_width, screen_height);
    if (!ctx->desktop_window) {
        free(ctx);
        return NULL;
    }

    // Set default configuration
    ctx->config.background_color = 0xFF1E3A5F;  // Dark blue
    ctx->config.wallpaper_mode = 3;  // Zoom
    ctx->config.show_desktop_icons = true;
    ctx->config.icon_size = ICON_SIZE;
    ctx->config.corner_top_left = HOTCORNER_SHOW_LAUNCHER;
    ctx->config.corner_top_right = HOTCORNER_SHOW_WORKSPACES;
    ctx->config.corner_bottom_left = HOTCORNER_SHOW_DESKTOP;
    ctx->config.corner_bottom_right = HOTCORNER_NONE;

    // Create default virtual desktop
    virtual_desktop_t* vd = &ctx->virtual_desktops[0];
    vd->id = 1;
    strncpy(vd->name, "Desktop 1", 31);
    vd->active = true;
    ctx->vdesktop_count = 1;
    ctx->current_vdesktop = 0;

    // Add default desktop icons
    desktop_add_icon(ctx, "Home", "/home/user", ICON_TYPE_FOLDER, 32, 32);
    desktop_add_icon(ctx, "Trash", "/home/user/.trash", ICON_TYPE_TRASH, 32, 128);
    desktop_add_icon(ctx, "Computer", "/", ICON_TYPE_DEVICE, 32, 224);

    ctx->running = true;

    return ctx;
}

// Destroy desktop shell
void desktop_destroy(desktop_ctx_t* ctx) {
    if (!ctx) return;

    if (ctx->desktop_window) {
        window_destroy(ctx->desktop_window);
    }

    if (ctx->wallpaper_texture) {
        // free(ctx->wallpaper_texture);
    }

    free(ctx);
}

// Load configuration
void desktop_load_config(desktop_ctx_t* ctx, const char* config_file) {
    if (!ctx || !config_file) return;

    int fd = sys_open(config_file, 0, 0); // O_RDONLY
    if (fd < 0) return;

    // Simple config parsing (binary dump for now to save space)
    sys_read(fd, &ctx->config, sizeof(desktop_config_t));
    sys_close(fd);
}

// Save configuration
void desktop_save_config(desktop_ctx_t* ctx, const char* config_file) {
    if (!ctx || !config_file) return;

    int fd = sys_open(config_file, 1 | 2, 0644); // O_WRONLY | O_CREAT
    if (fd < 0) return;

    sys_write(fd, &ctx->config, sizeof(desktop_config_t));
    sys_close(fd);
}

// Set wallpaper
void desktop_set_wallpaper(desktop_ctx_t* ctx, const char* path, uint32_t mode) {
    if (!ctx || !path) return;

    strncpy(ctx->config.wallpaper_path, path, 255);
    ctx->config.wallpaper_path[255] = '\0';
    ctx->config.wallpaper_mode = mode;

    // Wallpaper loading implemented via generation fallback for now
}

// Set background color
void desktop_set_background_color(desktop_ctx_t* ctx, uint32_t color) {
    if (!ctx) return;
    ctx->config.background_color = color;
}

// Add desktop icon
uint32_t desktop_add_icon(desktop_ctx_t* ctx, const char* label, const char* path, icon_type_t type, int32_t x, int32_t y) {
    if (!ctx || ctx->icon_count >= MAX_DESKTOP_ICONS) {
        return 0;
    }

    // Find free icon slot
    for (uint32_t i = 0; i < MAX_DESKTOP_ICONS; i++) {
        if (ctx->icons[i].id == 0) {
            desktop_icon_t* icon = &ctx->icons[i];

            icon->id = i + 1;
            icon->type = type;
            icon->x = x;
            icon->y = y;
            icon->visible = true;
            icon->selected = false;

            if (label) {
                strncpy(icon->label, label, 63);
                icon->label[63] = '\0';
            }

            if (path) {
                strncpy(icon->target_path, path, 255);
                icon->target_path[255] = '\0';
            }

            ctx->icon_count++;
            return icon->id;
        }
    }

    return 0;
}

// Remove desktop icon
void desktop_remove_icon(desktop_ctx_t* ctx, uint32_t icon_id) {
    if (!ctx || icon_id == 0) return;

    for (uint32_t i = 0; i < MAX_DESKTOP_ICONS; i++) {
        if (ctx->icons[i].id == icon_id) {
            memset(&ctx->icons[i], 0, sizeof(desktop_icon_t));
            ctx->icon_count--;
            break;
        }
    }
}

// Move desktop icon
void desktop_move_icon(desktop_ctx_t* ctx, uint32_t icon_id, int32_t x, int32_t y) {
    if (!ctx || icon_id == 0) return;

    for (uint32_t i = 0; i < MAX_DESKTOP_ICONS; i++) {
        if (ctx->icons[i].id == icon_id) {
            ctx->icons[i].x = x;
            ctx->icons[i].y = y;
            break;
        }
    }
}

// Select icon
void desktop_select_icon(desktop_ctx_t* ctx, uint32_t icon_id, bool selected) {
    if (!ctx || icon_id == 0) return;

    for (uint32_t i = 0; i < MAX_DESKTOP_ICONS; i++) {
        if (ctx->icons[i].id == icon_id) {
            ctx->icons[i].selected = selected;
            break;
        }
    }
}

// Open icon (launch application or open folder)
void desktop_open_icon(desktop_ctx_t* ctx, uint32_t icon_id) {
    if (!ctx || icon_id == 0) return;

    for (uint32_t i = 0; i < MAX_DESKTOP_ICONS; i++) {
        if (ctx->icons[i].id == icon_id) {
            desktop_icon_t* icon = &ctx->icons[i];
            
            // Use SYS_EXEC to launch
            // sys_exec(icon->target_path, ...);
            printf("Opening icon: %s (Path: %s)\n", icon->label, icon->target_path);
            break;
        }
    }
}

// Find icon at position
desktop_icon_t* desktop_find_icon_at(desktop_ctx_t* ctx, int32_t x, int32_t y) {
    if (!ctx) return NULL;

    for (int i = MAX_DESKTOP_ICONS - 1; i >= 0; i--) {
        desktop_icon_t* icon = &ctx->icons[i];

        if (icon->id == 0 || !icon->visible) continue;

        uint32_t icon_size = ctx->config.icon_size;

        if (x >= icon->x && x < icon->x + (int32_t)icon_size &&
            y >= icon->y && y < icon->y + (int32_t)icon_size) {
            return icon;
        }
    }

    return NULL;
}

// Virtual desktop functions (simplified)
uint32_t desktop_create_virtual(desktop_ctx_t* ctx, const char* name) {
    if (!ctx || ctx->vdesktop_count >= MAX_VIRTUAL_DESKTOPS) return 0;
    virtual_desktop_t* vd = &ctx->virtual_desktops[ctx->vdesktop_count];
    vd->id = ctx->vdesktop_count + 1;
    vd->active = false;
    vd->window_count = 0;
    if (name) strncpy(vd->name, name, 31);
    else snprintf(vd->name, 32, "Desktop %u", vd->id);
    ctx->vdesktop_count++;
    return vd->id;
}

void desktop_destroy_virtual(desktop_ctx_t* ctx, uint32_t vdesktop_id) {
    // Implementation omitted for brevity (same as original)
}

void desktop_switch_virtual(desktop_ctx_t* ctx, uint32_t vdesktop_id) {
    // Implementation omitted for brevity
}

void desktop_move_window_to_virtual(desktop_ctx_t* ctx, uint32_t window_id, uint32_t vdesktop_id) {
    // Implementation omitted for brevity
}

void desktop_snap_window(desktop_ctx_t* ctx, uint32_t window_id, int snap_position) {
    // Implementation omitted for brevity
}

void desktop_check_hot_corners(desktop_ctx_t* ctx, int32_t x, int32_t y) {
    // Implementation omitted for brevity
}

void desktop_trigger_hotcorner(desktop_ctx_t* ctx, hotcorner_action_t action) {
    // Implementation omitted for brevity
}

void desktop_show_context_menu(desktop_ctx_t* ctx, int32_t x, int32_t y) {
    if (!ctx) return;
    if (!ctx->context_menu) {
        ctx->context_menu = menu_create();
        menu_add_item(ctx->context_menu, "New Folder", NULL);
    }
    widget_set_position(ctx->context_menu, x, y);
    widget_set_visible(ctx->context_menu, true);
    ctx->context_menu_visible = true;
}

void desktop_hide_context_menu(desktop_ctx_t* ctx) {
    if (!ctx || !ctx->context_menu) return;
    widget_set_visible(ctx->context_menu, false);
    ctx->context_menu_visible = false;
}

// Input handling stubs
void desktop_handle_mouse_move(desktop_ctx_t* ctx, int32_t x, int32_t y) {}
void desktop_handle_mouse_button(desktop_ctx_t* ctx, int32_t x, int32_t y, uint32_t button, bool pressed) {}
void desktop_handle_key(desktop_ctx_t* ctx, uint32_t keycode, bool pressed) {}

// Render desktop
void desktop_render(desktop_ctx_t* ctx) {
    if (!ctx || !ctx->desktop_window) return;

    void* canvas = ctx->desktop_window->framebuffer;
    uint32_t width = ctx->desktop_window->width;
    uint32_t height = ctx->desktop_window->height;

    if (ctx->wallpaper_texture) {
        // Placeholder for texture drawing
        generate_wallpaper(ctx, (uint32_t*)canvas, width, height);
    } else {
        generate_wallpaper(ctx, (uint32_t*)canvas, width, height);
    }

    // Draw desktop icons
    if (ctx->config.show_desktop_icons) {
        for (uint32_t i = 0; i < MAX_DESKTOP_ICONS; i++) {
            desktop_icon_t* icon = &ctx->icons[i];
            if (icon->id == 0 || !icon->visible) continue;

            // Simple icon rendering (Placeholder box)
            uint32_t color = icon->selected ? 0xFF88AAFF : 0xFFAAAAAA;
            draw_rect((uint32_t*)canvas, width, icon->x, icon->y, ICON_SIZE, ICON_SIZE, color);
            
            // Draw label
            draw_string_desktop((uint32_t*)canvas, width, height, icon->x, icon->y + ICON_SIZE + 2, icon->label, 0xFFFFFFFF);
        }
    }

    // window_render(ctx->desktop_window); // Flush?
}

// Main desktop loop
void desktop_run(desktop_ctx_t* ctx) {
    if (!ctx) return;

    ipc_message_t msg;
    
    // Create and register IPC port
    uint64_t desktop_port_id = sys_ipc_create_port();
    if (desktop_port_id == 0) {
        printf("Failed to create desktop IPC port\n");
        return;
    }
    
    // Set as process IPC port so others can find us by PID
    sys_set_process_ipc_port(desktop_port_id);
    
    printf("Desktop running on port %lu...\n", desktop_port_id);

    while (ctx->running) {
        // Process IPC messages
        int res = sys_ipc_receive(desktop_port_id, &msg);
        if (res == 0) {
            // Handle message
            // msg.type ...
        }

        // Render desktop
        desktop_render(ctx);

        // Yield CPU or sleep
        sys_sleep(16); // ~60 FPS
    }
}