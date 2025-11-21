/**
 * @file taskbar.c
 * @brief Taskbar/Panel Implementation
 */

#include "taskbar.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Create taskbar
taskbar_ctx_t* taskbar_create(compositor_ctx_t* compositor) {
    taskbar_ctx_t* ctx = (taskbar_ctx_t*)malloc(sizeof(taskbar_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(taskbar_ctx_t));

    ctx->compositor = compositor;
    ctx->position = TASKBAR_POSITION_BOTTOM;
    ctx->height = TASKBAR_HEIGHT;

    // Create taskbar window
    uint32_t width = compositor->screen_width;
    uint32_t height = ctx->height;
    int32_t y = compositor->screen_height - height;

    ctx->taskbar_window = window_create("Taskbar", width, height);
    if (!ctx->taskbar_window) {
        free(ctx);
        return NULL;
    }

    // Create root panel for widgets
    widget_t* root = panel_create();
    widget_set_size(root, width, height);
    widget_set_colors(root, 0xFFFFFFFF, 0xFF2C3E50);  // White text, dark blue background
    ctx->taskbar_window->root = root;

    // Create launcher button
    ctx->launcher_button = button_create("Start");
    widget_set_position(ctx->launcher_button, 5, 5);
    widget_set_size(ctx->launcher_button, 60, 30);
    widget_set_click_handler(ctx->launcher_button, taskbar_launcher_clicked, ctx);
    widget_add_child(root, ctx->launcher_button);

    // Create clock label
    ctx->clock_label = label_create("00:00");
    widget_set_position(ctx->clock_label, width - 80, 10);
    widget_set_size(ctx->clock_label, 70, 20);
    widget_set_click_handler(ctx->clock_label, taskbar_clock_clicked, ctx);
    widget_add_child(root, ctx->clock_label);

    // Create volume button
    ctx->volume_button = button_create("Vol");
    widget_set_position(ctx->volume_button, width - 160, 5);
    widget_set_size(ctx->volume_button, 40, 30);
    widget_set_click_handler(ctx->volume_button, taskbar_volume_clicked, ctx);
    widget_add_child(root, ctx->volume_button);

    // Create network button
    ctx->network_button = button_create("Net");
    widget_set_position(ctx->network_button, width - 210, 5);
    widget_set_size(ctx->network_button, 40, 30);
    widget_set_click_handler(ctx->network_button, taskbar_network_clicked, ctx);
    widget_add_child(root, ctx->network_button);

    // Create battery button (if applicable)
    ctx->battery_button = button_create("Bat");
    widget_set_position(ctx->battery_button, width - 260, 5);
    widget_set_size(ctx->battery_button, 40, 30);
    widget_set_click_handler(ctx->battery_button, taskbar_battery_clicked, ctx);
    widget_add_child(root, ctx->battery_button);
    widget_set_visible(ctx->battery_button, false);  // Hidden by default

    // Initialize system status
    ctx->status.volume = 50;
    ctx->status.muted = false;
    ctx->status.network_connected = false;
    ctx->status.has_battery = false;

    ctx->running = true;

    return ctx;
}

// Destroy taskbar
void taskbar_destroy(taskbar_ctx_t* ctx) {
    if (!ctx) return;

    if (ctx->taskbar_window) {
        window_destroy(ctx->taskbar_window);
    }

    // TODO: Destroy popups

    free(ctx);
}

// Add window to taskbar
void taskbar_add_window(taskbar_ctx_t* ctx, uint32_t window_id, const char* title) {
    if (!ctx || ctx->window_count >= MAX_TASKBAR_WINDOWS) {
        return;
    }

    // Find free slot
    for (uint32_t i = 0; i < MAX_TASKBAR_WINDOWS; i++) {
        if (ctx->windows[i].window_id == 0) {
            taskbar_window_t* win = &ctx->windows[i];

            win->window_id = window_id;
            win->active = false;

            if (title) {
                strncpy(win->title, title, 127);
                win->title[127] = '\0';
            }

            // Create button for this window
            win->button = button_create(win->title);
            widget_set_size(win->button, 150, 30);
            widget_set_click_handler(win->button, (event_callback_t)taskbar_window_clicked, (void*)(uintptr_t)window_id);

            // Position button in window list area
            int32_t x = 75 + (i * 160);  // After launcher button
            widget_set_position(win->button, x, 5);

            widget_add_child(ctx->taskbar_window->root, win->button);

            ctx->window_count++;
            break;
        }
    }
}

// Remove window from taskbar
void taskbar_remove_window(taskbar_ctx_t* ctx, uint32_t window_id) {
    if (!ctx) return;

    for (uint32_t i = 0; i < MAX_TASKBAR_WINDOWS; i++) {
        if (ctx->windows[i].window_id == window_id) {
            taskbar_window_t* win = &ctx->windows[i];

            // Remove button widget
            if (win->button) {
                widget_remove_child(ctx->taskbar_window->root, win->button);
                widget_destroy(win->button);
            }

            // TODO: Free thumbnail

            memset(win, 0, sizeof(taskbar_window_t));
            ctx->window_count--;

            // Reposition remaining window buttons
            uint32_t pos = 0;
            for (uint32_t j = 0; j < MAX_TASKBAR_WINDOWS; j++) {
                if (ctx->windows[j].window_id != 0) {
                    int32_t x = 75 + (pos * 160);
                    widget_set_position(ctx->windows[j].button, x, 5);
                    pos++;
                }
            }

            break;
        }
    }
}

// Update window title
void taskbar_update_window(taskbar_ctx_t* ctx, uint32_t window_id, const char* title) {
    if (!ctx || !title) return;

    for (uint32_t i = 0; i < MAX_TASKBAR_WINDOWS; i++) {
        if (ctx->windows[i].window_id == window_id) {
            strncpy(ctx->windows[i].title, title, 127);
            ctx->windows[i].title[127] = '\0';

            if (ctx->windows[i].button) {
                button_set_text(ctx->windows[i].button, title);
            }

            break;
        }
    }
}

// Set active window
void taskbar_set_active_window(taskbar_ctx_t* ctx, uint32_t window_id) {
    if (!ctx) return;

    for (uint32_t i = 0; i < MAX_TASKBAR_WINDOWS; i++) {
        if (ctx->windows[i].window_id != 0) {
            ctx->windows[i].active = (ctx->windows[i].window_id == window_id);

            // Update button appearance
            if (ctx->windows[i].button) {
                uint32_t bg_color = ctx->windows[i].active ? 0xFF3498DB : 0xFF34495E;
                widget_set_colors(ctx->windows[i].button, 0xFFFFFFFF, bg_color);
            }
        }
    }
}

// Window button clicked
void taskbar_window_clicked(taskbar_ctx_t* ctx, uint32_t window_id) {
    if (!ctx) return;

    // Focus/raise the window in compositor
    compositor_focus_window(ctx->compositor, window_id);
}

// Add system tray icon
uint32_t taskbar_add_tray_icon(taskbar_ctx_t* ctx, uint32_t pid, const char* tooltip, void* icon) {
    if (!ctx || ctx->tray_count >= MAX_TRAY_ICONS) {
        return 0;
    }

    tray_icon_t* tray = &ctx->tray_icons[ctx->tray_count];
    tray->id = ctx->tray_count + 1;
    tray->owner_pid = pid;
    tray->icon = icon;

    if (tooltip) {
        strncpy(tray->tooltip, tooltip, 63);
        tray->tooltip[63] = '\0';
    }

    // Create button for tray icon
    tray->button = button_create("");
    widget_set_size(tray->button, 32, 32);

    // Position in system tray area (left of clock)
    int32_t x = ctx->taskbar_window->width - 280 - (ctx->tray_count * 36);
    widget_set_position(tray->button, x, 4);

    widget_add_child(ctx->taskbar_window->root, tray->button);

    ctx->tray_count++;
    return tray->id;
}

// Remove tray icon
void taskbar_remove_tray_icon(taskbar_ctx_t* ctx, uint32_t tray_id) {
    if (!ctx) return;

    for (uint32_t i = 0; i < MAX_TRAY_ICONS; i++) {
        if (ctx->tray_icons[i].id == tray_id) {
            if (ctx->tray_icons[i].button) {
                widget_remove_child(ctx->taskbar_window->root, ctx->tray_icons[i].button);
                widget_destroy(ctx->tray_icons[i].button);
            }

            memmove(&ctx->tray_icons[i],
                    &ctx->tray_icons[i + 1],
                    (ctx->tray_count - i - 1) * sizeof(tray_icon_t));
            ctx->tray_count--;

            // Reposition remaining icons
            for (uint32_t j = 0; j < ctx->tray_count; j++) {
                int32_t x = ctx->taskbar_window->width - 280 - (j * 36);
                widget_set_position(ctx->tray_icons[j].button, x, 4);
            }

            break;
        }
    }
}

// Update time display
void taskbar_update_time(taskbar_ctx_t* ctx) {
    if (!ctx) return;

    // TODO: Get current time from system
    // For now, use placeholder
    char time_str[32];
    snprintf(time_str, sizeof(time_str), "%02u:%02u", ctx->status.hour, ctx->status.minute);

    label_set_text(ctx->clock_label, time_str);
}

// Update volume status
void taskbar_update_volume(taskbar_ctx_t* ctx, uint8_t volume, bool muted) {
    if (!ctx) return;

    ctx->status.volume = volume;
    ctx->status.muted = muted;

    // Update volume button icon/text
    char vol_text[16];
    if (muted) {
        snprintf(vol_text, sizeof(vol_text), "Mute");
    } else {
        snprintf(vol_text, sizeof(vol_text), "Vol %u%%", volume);
    }

    button_set_text(ctx->volume_button, vol_text);
}

// Update network status
void taskbar_update_network(taskbar_ctx_t* ctx, bool connected, const char* ssid, uint8_t signal) {
    if (!ctx) return;

    ctx->status.network_connected = connected;
    ctx->status.network_signal = signal;

    if (ssid) {
        strncpy(ctx->status.network_ssid, ssid, 31);
        ctx->status.network_ssid[31] = '\0';
    }

    // Update network button
    if (connected) {
        button_set_text(ctx->network_button, "Wifi");
    } else {
        button_set_text(ctx->network_button, "Off");
    }
}

// Update battery status
void taskbar_update_battery(taskbar_ctx_t* ctx, uint8_t level, bool charging) {
    if (!ctx) return;

    ctx->status.has_battery = true;
    ctx->status.battery_level = level;
    ctx->status.charging = charging;

    // Show battery button
    widget_set_visible(ctx->battery_button, true);

    // Update battery button
    char bat_text[16];
    snprintf(bat_text, sizeof(bat_text), "%u%%", level);
    button_set_text(ctx->battery_button, bat_text);
}

// Show calendar popup
void taskbar_show_calendar(taskbar_ctx_t* ctx) {
    if (!ctx) return;

    // TODO: Create calendar widget popup
    ctx->calendar_visible = true;
}

// Hide calendar popup
void taskbar_hide_calendar(taskbar_ctx_t* ctx) {
    if (!ctx) return;
    ctx->calendar_visible = false;
}

// Show volume popup
void taskbar_show_volume(taskbar_ctx_t* ctx) {
    if (!ctx) return;

    // TODO: Create volume slider popup
    ctx->volume_visible = true;
}

// Hide volume popup
void taskbar_hide_volume(taskbar_ctx_t* ctx) {
    if (!ctx) return;
    ctx->volume_visible = false;
}

// Show network popup
void taskbar_show_network(taskbar_ctx_t* ctx) {
    if (!ctx) return;

    // TODO: Create network list popup
    ctx->network_visible = true;
}

// Hide network popup
void taskbar_hide_network(taskbar_ctx_t* ctx) {
    if (!ctx) return;
    ctx->network_visible = false;
}

// Launcher button clicked
void taskbar_launcher_clicked(widget_t* widget, void* userdata) {
    taskbar_ctx_t* ctx = (taskbar_ctx_t*)userdata;
    if (!ctx) return;

    // TODO: Launch application launcher
}

// Clock clicked
void taskbar_clock_clicked(widget_t* widget, void* userdata) {
    taskbar_ctx_t* ctx = (taskbar_ctx_t*)userdata;
    if (!ctx) return;

    if (ctx->calendar_visible) {
        taskbar_hide_calendar(ctx);
    } else {
        taskbar_show_calendar(ctx);
    }
}

// Volume clicked
void taskbar_volume_clicked(widget_t* widget, void* userdata) {
    taskbar_ctx_t* ctx = (taskbar_ctx_t*)userdata;
    if (!ctx) return;

    if (ctx->volume_visible) {
        taskbar_hide_volume(ctx);
    } else {
        taskbar_show_volume(ctx);
    }
}

// Network clicked
void taskbar_network_clicked(widget_t* widget, void* userdata) {
    taskbar_ctx_t* ctx = (taskbar_ctx_t*)userdata;
    if (!ctx) return;

    if (ctx->network_visible) {
        taskbar_hide_network(ctx);
    } else {
        taskbar_show_network(ctx);
    }
}

// Battery clicked
void taskbar_battery_clicked(widget_t* widget, void* userdata) {
    taskbar_ctx_t* ctx = (taskbar_ctx_t*)userdata;
    if (!ctx) return;

    // TODO: Show battery settings
}

// Render taskbar
void taskbar_render(taskbar_ctx_t* ctx) {
    if (!ctx || !ctx->taskbar_window) return;

    // Update time every second
    taskbar_update_time(ctx);

    // Render taskbar window
    window_render(ctx->taskbar_window);

    // Render popups if visible
    if (ctx->calendar_visible && ctx->calendar_popup) {
        widget_paint(ctx->calendar_popup, ctx->taskbar_window->framebuffer);
    }

    if (ctx->volume_visible && ctx->volume_popup) {
        widget_paint(ctx->volume_popup, ctx->taskbar_window->framebuffer);
    }

    if (ctx->network_visible && ctx->network_popup) {
        widget_paint(ctx->network_popup, ctx->taskbar_window->framebuffer);
    }
}

// Main taskbar loop
void taskbar_run(taskbar_ctx_t* ctx) {
    if (!ctx) return;

    while (ctx->running) {
        // TODO: Process IPC messages from compositor and system services

        // Render taskbar
        taskbar_render(ctx);

        // TODO: Sleep or yield CPU
    }
}
