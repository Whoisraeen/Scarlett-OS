/**
 * @file taskbar.h
 * @brief Taskbar/Panel for Scarlett OS
 *
 * Provides window list, system tray, clock, and quick access to system functions
 */

#ifndef APPS_TASKBAR_H
#define APPS_TASKBAR_H

#include <stdint.h>
#include <stdbool.h>
#include "../../gui/compositor/src/compositor.h"
#include "../../gui/widgets/src/widgets.h"

// Taskbar configuration
#define MAX_TASKBAR_WINDOWS 64
#define MAX_TRAY_ICONS 16
#define TASKBAR_HEIGHT 40
#define TASKBAR_POSITION_BOTTOM 0
#define TASKBAR_POSITION_TOP 1
#define TASKBAR_POSITION_LEFT 2
#define TASKBAR_POSITION_RIGHT 3

// Taskbar window item
typedef struct {
    uint32_t window_id;
    char title[128];
    void* thumbnail;  // Window thumbnail
    bool active;
    widget_t* button;
} taskbar_window_t;

// System tray icon
typedef struct {
    uint32_t id;
    char tooltip[64];
    void* icon;
    uint32_t owner_pid;
    widget_t* button;
} tray_icon_t;

// System status
typedef struct {
    // Network
    bool network_connected;
    char network_ssid[32];
    uint8_t network_signal;  // 0-100

    // Audio
    uint8_t volume;  // 0-100
    bool muted;

    // Battery
    bool has_battery;
    uint8_t battery_level;  // 0-100
    bool charging;

    // Time
    uint32_t hour;
    uint32_t minute;
    uint32_t second;
    char date_str[32];  // "Mon, Jan 1, 2025"
} system_status_t;

// Taskbar context
typedef struct {
    compositor_ctx_t* compositor;
    window_t* taskbar_window;

    taskbar_window_t windows[MAX_TASKBAR_WINDOWS];
    uint32_t window_count;

    tray_icon_t tray_icons[MAX_TRAY_ICONS];
    uint32_t tray_count;

    system_status_t status;

    // Widgets
    widget_t* launcher_button;
    widget_t* workspace_switcher;
    widget_t* clock_label;
    widget_t* volume_button;
    widget_t* network_button;
    widget_t* battery_button;

    // Popups
    widget_t* calendar_popup;
    widget_t* volume_popup;
    widget_t* network_popup;
    bool calendar_visible;
    bool volume_visible;
    bool network_visible;

    uint32_t position;  // TASKBAR_POSITION_*
    uint32_t height;

    bool running;
} taskbar_ctx_t;

// Taskbar initialization
taskbar_ctx_t* taskbar_create(compositor_ctx_t* compositor);
void taskbar_destroy(taskbar_ctx_t* ctx);
void taskbar_run(taskbar_ctx_t* ctx);

// Window list management
void taskbar_add_window(taskbar_ctx_t* ctx, uint32_t window_id, const char* title);
void taskbar_remove_window(taskbar_ctx_t* ctx, uint32_t window_id);
void taskbar_update_window(taskbar_ctx_t* ctx, uint32_t window_id, const char* title);
void taskbar_set_active_window(taskbar_ctx_t* ctx, uint32_t window_id);
void taskbar_window_clicked(taskbar_ctx_t* ctx, uint32_t window_id);

// System tray
uint32_t taskbar_add_tray_icon(taskbar_ctx_t* ctx, uint32_t pid, const char* tooltip, void* icon);
void taskbar_remove_tray_icon(taskbar_ctx_t* ctx, uint32_t tray_id);
void taskbar_update_tray_icon(taskbar_ctx_t* ctx, uint32_t tray_id, void* icon);

// System status updates
void taskbar_update_time(taskbar_ctx_t* ctx);
void taskbar_update_volume(taskbar_ctx_t* ctx, uint8_t volume, bool muted);
void taskbar_update_network(taskbar_ctx_t* ctx, bool connected, const char* ssid, uint8_t signal);
void taskbar_update_battery(taskbar_ctx_t* ctx, uint8_t level, bool charging);

// Popup management
void taskbar_show_calendar(taskbar_ctx_t* ctx);
void taskbar_hide_calendar(taskbar_ctx_t* ctx);
void taskbar_show_volume(taskbar_ctx_t* ctx);
void taskbar_hide_volume(taskbar_ctx_t* ctx);
void taskbar_show_network(taskbar_ctx_t* ctx);
void taskbar_hide_network(taskbar_ctx_t* ctx);

// Callbacks
void taskbar_launcher_clicked(widget_t* widget, void* userdata);
void taskbar_clock_clicked(widget_t* widget, void* userdata);
void taskbar_volume_clicked(widget_t* widget, void* userdata);
void taskbar_network_clicked(widget_t* widget, void* userdata);
void taskbar_battery_clicked(widget_t* widget, void* userdata);

// Rendering
void taskbar_render(taskbar_ctx_t* ctx);

#endif // APPS_TASKBAR_H
