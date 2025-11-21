/**
 * @file desktop.c
 * @brief Desktop Shell Implementation
 */

#include "desktop.h"
#include "../../gui/ugal/src/ugal.h"
#include <string.h>
#include <stdlib.h>

// Create desktop shell
desktop_ctx_t* desktop_create(compositor_ctx_t* compositor) {
    desktop_ctx_t* ctx = (desktop_ctx_t*)malloc(sizeof(desktop_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(desktop_ctx_t));

    ctx->compositor = compositor;

    // Create fullscreen desktop window
    ctx->desktop_window = window_create("Desktop", compositor->screen_width, compositor->screen_height);
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
        // TODO: Free wallpaper texture
    }

    free(ctx);
}

// Load configuration
void desktop_load_config(desktop_ctx_t* ctx, const char* config_file) {
    if (!ctx || !config_file) return;

    // TODO: Load config from file via VFS
    // For now, use defaults
}

// Save configuration
void desktop_save_config(desktop_ctx_t* ctx, const char* config_file) {
    if (!ctx || !config_file) return;

    // TODO: Save config to file via VFS
}

// Set wallpaper
void desktop_set_wallpaper(desktop_ctx_t* ctx, const char* path, uint32_t mode) {
    if (!ctx || !path) return;

    strncpy(ctx->config.wallpaper_path, path, 255);
    ctx->config.wallpaper_path[255] = '\0';
    ctx->config.wallpaper_mode = mode;

    // TODO: Load wallpaper image
    // ugal_load_texture(ctx->compositor->gpu_device, path, &ctx->wallpaper_texture);
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

            // TODO: Load icon image based on type

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
            // TODO: Free icon image

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

            // TODO: Open the icon based on type
            // - ICON_TYPE_APPLICATION: Launch application
            // - ICON_TYPE_FOLDER: Open file manager
            // - ICON_TYPE_FILE: Open with default application
            // - ICON_TYPE_TRASH: Open trash folder

            break;
        }
    }
}

// Find icon at position
desktop_icon_t* desktop_find_icon_at(desktop_ctx_t* ctx, int32_t x, int32_t y) {
    if (!ctx) return NULL;

    // Check icons in reverse order (top to bottom)
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

// Create virtual desktop
uint32_t desktop_create_virtual(desktop_ctx_t* ctx, const char* name) {
    if (!ctx || ctx->vdesktop_count >= MAX_VIRTUAL_DESKTOPS) {
        return 0;
    }

    virtual_desktop_t* vd = &ctx->virtual_desktops[ctx->vdesktop_count];
    vd->id = ctx->vdesktop_count + 1;
    vd->active = false;
    vd->window_count = 0;

    if (name) {
        strncpy(vd->name, name, 31);
        vd->name[31] = '\0';
    } else {
        snprintf(vd->name, 32, "Desktop %u", vd->id);
    }

    ctx->vdesktop_count++;
    return vd->id;
}

// Destroy virtual desktop
void desktop_destroy_virtual(desktop_ctx_t* ctx, uint32_t vdesktop_id) {
    if (!ctx || vdesktop_id == 0 || ctx->vdesktop_count <= 1) return;

    // Don't allow destroying the last virtual desktop
    for (uint32_t i = 0; i < ctx->vdesktop_count; i++) {
        if (ctx->virtual_desktops[i].id == vdesktop_id) {
            // Move windows to desktop 1
            virtual_desktop_t* vd = &ctx->virtual_desktops[i];
            for (uint32_t j = 0; j < vd->window_count; j++) {
                desktop_move_window_to_virtual(ctx, vd->window_ids[j], 1);
            }

            // Remove this virtual desktop
            memmove(&ctx->virtual_desktops[i],
                    &ctx->virtual_desktops[i + 1],
                    (ctx->vdesktop_count - i - 1) * sizeof(virtual_desktop_t));
            ctx->vdesktop_count--;

            break;
        }
    }
}

// Switch to virtual desktop
void desktop_switch_virtual(desktop_ctx_t* ctx, uint32_t vdesktop_id) {
    if (!ctx || vdesktop_id == 0) return;

    for (uint32_t i = 0; i < ctx->vdesktop_count; i++) {
        if (ctx->virtual_desktops[i].id == vdesktop_id) {
            // Deactivate current desktop
            ctx->virtual_desktops[ctx->current_vdesktop].active = false;

            // Activate new desktop
            ctx->virtual_desktops[i].active = true;
            ctx->current_vdesktop = i;

            // Hide windows from old desktop, show windows from new desktop
            // TODO: Update window visibility via compositor

            break;
        }
    }
}

// Move window to virtual desktop
void desktop_move_window_to_virtual(desktop_ctx_t* ctx, uint32_t window_id, uint32_t vdesktop_id) {
    if (!ctx || window_id == 0 || vdesktop_id == 0) return;

    // Remove from all virtual desktops
    for (uint32_t i = 0; i < ctx->vdesktop_count; i++) {
        virtual_desktop_t* vd = &ctx->virtual_desktops[i];
        for (uint32_t j = 0; j < vd->window_count; j++) {
            if (vd->window_ids[j] == window_id) {
                memmove(&vd->window_ids[j],
                        &vd->window_ids[j + 1],
                        (vd->window_count - j - 1) * sizeof(uint32_t));
                vd->window_count--;
                break;
            }
        }
    }

    // Add to target virtual desktop
    for (uint32_t i = 0; i < ctx->vdesktop_count; i++) {
        if (ctx->virtual_desktops[i].id == vdesktop_id) {
            virtual_desktop_t* vd = &ctx->virtual_desktops[i];
            if (vd->window_count < 256) {
                vd->window_ids[vd->window_count++] = window_id;
            }
            break;
        }
    }
}

// Snap window to screen edge
void desktop_snap_window(desktop_ctx_t* ctx, uint32_t window_id, int snap_position) {
    if (!ctx || window_id == 0) return;

    uint32_t screen_w = ctx->compositor->screen_width;
    uint32_t screen_h = ctx->compositor->screen_height;
    int32_t x = 0, y = 0;
    uint32_t w = 0, h = 0;

    switch (snap_position) {
        case SNAP_LEFT:
            x = 0; y = 0; w = screen_w / 2; h = screen_h;
            break;
        case SNAP_RIGHT:
            x = screen_w / 2; y = 0; w = screen_w / 2; h = screen_h;
            break;
        case SNAP_TOP:
            x = 0; y = 0; w = screen_w; h = screen_h / 2;
            break;
        case SNAP_BOTTOM:
            x = 0; y = screen_h / 2; w = screen_w; h = screen_h / 2;
            break;
        case SNAP_TOPLEFT:
            x = 0; y = 0; w = screen_w / 2; h = screen_h / 2;
            break;
        case SNAP_TOPRIGHT:
            x = screen_w / 2; y = 0; w = screen_w / 2; h = screen_h / 2;
            break;
        case SNAP_BOTTOMLEFT:
            x = 0; y = screen_h / 2; w = screen_w / 2; h = screen_h / 2;
            break;
        case SNAP_BOTTOMRIGHT:
            x = screen_w / 2; y = screen_h / 2; w = screen_w / 2; h = screen_h / 2;
            break;
        case SNAP_MAXIMIZE:
            x = 0; y = 0; w = screen_w; h = screen_h;
            break;
        default:
            return;
    }

    compositor_move_window(ctx->compositor, window_id, x, y);
    compositor_resize_window(ctx->compositor, window_id, w, h);
}

// Check hot corners
void desktop_check_hot_corners(desktop_ctx_t* ctx, int32_t x, int32_t y) {
    if (!ctx) return;

    const int32_t corner_size = 5;  // 5 pixel corner trigger area

    // Top-left
    if (x < corner_size && y < corner_size && ctx->config.corner_top_left != HOTCORNER_NONE) {
        desktop_trigger_hotcorner(ctx, ctx->config.corner_top_left);
    }
    // Top-right
    else if (x >= (int32_t)(ctx->compositor->screen_width - corner_size) && y < corner_size &&
             ctx->config.corner_top_right != HOTCORNER_NONE) {
        desktop_trigger_hotcorner(ctx, ctx->config.corner_top_right);
    }
    // Bottom-left
    else if (x < corner_size && y >= (int32_t)(ctx->compositor->screen_height - corner_size) &&
             ctx->config.corner_bottom_left != HOTCORNER_NONE) {
        desktop_trigger_hotcorner(ctx, ctx->config.corner_bottom_left);
    }
    // Bottom-right
    else if (x >= (int32_t)(ctx->compositor->screen_width - corner_size) &&
             y >= (int32_t)(ctx->compositor->screen_height - corner_size) &&
             ctx->config.corner_bottom_right != HOTCORNER_NONE) {
        desktop_trigger_hotcorner(ctx, ctx->config.corner_bottom_right);
    }
}

// Trigger hot corner action
void desktop_trigger_hotcorner(desktop_ctx_t* ctx, hotcorner_action_t action) {
    if (!ctx) return;

    switch (action) {
        case HOTCORNER_SHOW_DESKTOP:
            // Minimize all windows
            // TODO: Implement via compositor
            break;

        case HOTCORNER_SHOW_LAUNCHER:
            // Launch application launcher
            // TODO: Implement
            break;

        case HOTCORNER_SHOW_WORKSPACES:
            // Show virtual desktop switcher
            // TODO: Implement
            break;

        case HOTCORNER_LOCK_SCREEN:
            // Lock the screen
            // TODO: Implement
            break;

        default:
            break;
    }
}

// Show context menu
void desktop_show_context_menu(desktop_ctx_t* ctx, int32_t x, int32_t y) {
    if (!ctx) return;

    // Create context menu if needed
    if (!ctx->context_menu) {
        ctx->context_menu = menu_create();

        menu_add_item(ctx->context_menu, "New Folder", NULL);
        menu_add_item(ctx->context_menu, "New File", NULL);
        menu_add_separator(ctx->context_menu);
        menu_add_item(ctx->context_menu, "Paste", NULL);
        menu_add_separator(ctx->context_menu);
        menu_add_item(ctx->context_menu, "Display Settings", NULL);
        menu_add_item(ctx->context_menu, "Personalize", NULL);
    }

    widget_set_position(ctx->context_menu, x, y);
    widget_set_visible(ctx->context_menu, true);
    ctx->context_menu_visible = true;
}

// Hide context menu
void desktop_hide_context_menu(desktop_ctx_t* ctx) {
    if (!ctx || !ctx->context_menu) return;

    widget_set_visible(ctx->context_menu, false);
    ctx->context_menu_visible = false;
}

// Handle mouse move
void desktop_handle_mouse_move(desktop_ctx_t* ctx, int32_t x, int32_t y) {
    if (!ctx) return;

    // Check hot corners
    desktop_check_hot_corners(ctx, x, y);

    // Handle icon dragging
    if (ctx->dragging_icon) {
        int32_t new_x = x - ctx->drag_offset_x;
        int32_t new_y = y - ctx->drag_offset_y;
        desktop_move_icon(ctx, ctx->dragged_icon_id, new_x, new_y);
    }
}

// Handle mouse button
void desktop_handle_mouse_button(desktop_ctx_t* ctx, int32_t x, int32_t y, uint32_t button, bool pressed) {
    if (!ctx) return;

    if (button == 1) {  // Left click
        if (pressed) {
            // Check if clicking on icon
            desktop_icon_t* icon = desktop_find_icon_at(ctx, x, y);

            if (icon) {
                // Deselect all other icons
                for (uint32_t i = 0; i < MAX_DESKTOP_ICONS; i++) {
                    if (ctx->icons[i].id != 0 && ctx->icons[i].id != icon->id) {
                        ctx->icons[i].selected = false;
                    }
                }

                // Select this icon
                icon->selected = true;

                // Start dragging
                ctx->dragging_icon = true;
                ctx->dragged_icon_id = icon->id;
                ctx->drag_offset_x = x - icon->x;
                ctx->drag_offset_y = y - icon->y;
            } else {
                // Clicked on empty space - deselect all icons
                for (uint32_t i = 0; i < MAX_DESKTOP_ICONS; i++) {
                    ctx->icons[i].selected = false;
                }

                // Hide context menu if visible
                if (ctx->context_menu_visible) {
                    desktop_hide_context_menu(ctx);
                }
            }
        } else {
            // Released left button
            if (ctx->dragging_icon) {
                ctx->dragging_icon = false;
            }
        }
    } else if (button == 2) {  // Right click
        if (pressed) {
            desktop_show_context_menu(ctx, x, y);
        }
    }
}

// Handle keyboard
void desktop_handle_key(desktop_ctx_t* ctx, uint32_t keycode, bool pressed) {
    if (!ctx || !pressed) return;

    // TODO: Implement keyboard shortcuts
    // - Ctrl+Alt+Left/Right: Switch virtual desktop
    // - Win+D: Show desktop
    // - Win+L: Lock screen
    // - Delete: Delete selected icons
}

// Render desktop
void desktop_render(desktop_ctx_t* ctx) {
    if (!ctx || !ctx->desktop_window) return;

    void* canvas = ctx->desktop_window->framebuffer;

    // TODO: Draw wallpaper or background color

    // Draw desktop icons
    if (ctx->config.show_desktop_icons) {
        for (uint32_t i = 0; i < MAX_DESKTOP_ICONS; i++) {
            desktop_icon_t* icon = &ctx->icons[i];

            if (icon->id == 0 || !icon->visible) continue;

            // TODO: Draw icon image and label
            // - Draw icon at (icon->x, icon->y)
            // - Draw label below icon
            // - Draw selection highlight if icon->selected
        }
    }

    // Render context menu if visible
    if (ctx->context_menu_visible && ctx->context_menu) {
        widget_paint(ctx->context_menu, canvas);
    }

    window_render(ctx->desktop_window);
}

// Main desktop loop
void desktop_run(desktop_ctx_t* ctx) {
    if (!ctx) return;

    while (ctx->running) {
        // TODO: Process IPC messages from compositor

        // Render desktop
        desktop_render(ctx);

        // TODO: Yield CPU or sleep
    }
}
