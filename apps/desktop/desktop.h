/**
 * @file desktop.h
 * @brief Desktop Shell for Scarlett OS
 *
 * Manages wallpapers, icons, virtual desktops, and desktop interactions
 */

#ifndef APPS_DESKTOP_H
#define APPS_DESKTOP_H

#include <stdint.h>
#include <stdbool.h>
#include "../../gui/compositor/src/compositor.h"
#include "../../gui/widgets/src/widgets.h"

// Desktop configuration
#define MAX_DESKTOP_ICONS 128
#define MAX_VIRTUAL_DESKTOPS 16
#define ICON_SIZE 64
#define ICON_SPACING 16

// Icon types
typedef enum {
    ICON_TYPE_FILE,
    ICON_TYPE_FOLDER,
    ICON_TYPE_APPLICATION,
    ICON_TYPE_TRASH,
    ICON_TYPE_DEVICE,
} icon_type_t;

// Desktop icon
typedef struct {
    uint32_t id;
    char label[64];
    icon_type_t type;
    int32_t x, y;
    char target_path[256];
    void* icon_image;
    bool selected;
    bool visible;
} desktop_icon_t;

// Virtual desktop
typedef struct {
    uint32_t id;
    char name[32];
    uint32_t window_ids[256];
    uint32_t window_count;
    bool active;
} virtual_desktop_t;

// Hot corner actions
typedef enum {
    HOTCORNER_NONE = 0,
    HOTCORNER_SHOW_DESKTOP = 1,
    HOTCORNER_SHOW_LAUNCHER = 2,
    HOTCORNER_SHOW_WORKSPACES = 3,
    HOTCORNER_LOCK_SCREEN = 4,
} hotcorner_action_t;

// Desktop configuration
typedef struct {
    char wallpaper_path[256];
    uint32_t wallpaper_mode;  // 0=center, 1=stretch, 2=tile, 3=zoom
    uint32_t background_color;
    hotcorner_action_t corner_top_left;
    hotcorner_action_t corner_top_right;
    hotcorner_action_t corner_bottom_left;
    hotcorner_action_t corner_bottom_right;
    bool show_desktop_icons;
    uint32_t icon_size;
} desktop_config_t;

// Desktop shell context
typedef struct {
    compositor_ctx_t* compositor;
    window_t* desktop_window;

    desktop_icon_t icons[MAX_DESKTOP_ICONS];
    uint32_t icon_count;

    virtual_desktop_t virtual_desktops[MAX_VIRTUAL_DESKTOPS];
    uint32_t vdesktop_count;
    uint32_t current_vdesktop;

    desktop_config_t config;

    void* wallpaper_texture;

    // Drag & drop state
    bool dragging_icon;
    uint32_t dragged_icon_id;
    int32_t drag_offset_x, drag_offset_y;

    // Context menu
    widget_t* context_menu;
    bool context_menu_visible;

    bool running;
} desktop_ctx_t;

// Desktop initialization
desktop_ctx_t* desktop_create(compositor_ctx_t* compositor);
void desktop_destroy(desktop_ctx_t* ctx);
void desktop_run(desktop_ctx_t* ctx);

// Desktop configuration
void desktop_load_config(desktop_ctx_t* ctx, const char* config_file);
void desktop_save_config(desktop_ctx_t* ctx, const char* config_file);
void desktop_set_wallpaper(desktop_ctx_t* ctx, const char* path, uint32_t mode);
void desktop_set_background_color(desktop_ctx_t* ctx, uint32_t color);

// Icon management
uint32_t desktop_add_icon(desktop_ctx_t* ctx, const char* label, const char* path, icon_type_t type, int32_t x, int32_t y);
void desktop_remove_icon(desktop_ctx_t* ctx, uint32_t icon_id);
void desktop_move_icon(desktop_ctx_t* ctx, uint32_t icon_id, int32_t x, int32_t y);
void desktop_select_icon(desktop_ctx_t* ctx, uint32_t icon_id, bool selected);
void desktop_open_icon(desktop_ctx_t* ctx, uint32_t icon_id);
desktop_icon_t* desktop_find_icon_at(desktop_ctx_t* ctx, int32_t x, int32_t y);

// Virtual desktops
uint32_t desktop_create_virtual(desktop_ctx_t* ctx, const char* name);
void desktop_destroy_virtual(desktop_ctx_t* ctx, uint32_t vdesktop_id);
void desktop_switch_virtual(desktop_ctx_t* ctx, uint32_t vdesktop_id);
void desktop_move_window_to_virtual(desktop_ctx_t* ctx, uint32_t window_id, uint32_t vdesktop_id);

// Window snapping
void desktop_snap_window(desktop_ctx_t* ctx, uint32_t window_id, int snap_position);
#define SNAP_LEFT 1
#define SNAP_RIGHT 2
#define SNAP_TOP 3
#define SNAP_BOTTOM 4
#define SNAP_TOPLEFT 5
#define SNAP_TOPRIGHT 6
#define SNAP_BOTTOMLEFT 7
#define SNAP_BOTTOMRIGHT 8
#define SNAP_MAXIMIZE 9

// Hot corners
void desktop_check_hot_corners(desktop_ctx_t* ctx, int32_t x, int32_t y);
void desktop_trigger_hotcorner(desktop_ctx_t* ctx, hotcorner_action_t action);

// Context menu
void desktop_show_context_menu(desktop_ctx_t* ctx, int32_t x, int32_t y);
void desktop_hide_context_menu(desktop_ctx_t* ctx);

// Input handling
void desktop_handle_mouse_move(desktop_ctx_t* ctx, int32_t x, int32_t y);
void desktop_handle_mouse_button(desktop_ctx_t* ctx, int32_t x, int32_t y, uint32_t button, bool pressed);
void desktop_handle_key(desktop_ctx_t* ctx, uint32_t keycode, bool pressed);

// Rendering
void desktop_render(desktop_ctx_t* ctx);

#endif // APPS_DESKTOP_H
