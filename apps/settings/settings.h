/**
 * @file settings.h
 * @brief System Settings Application for Scarlett OS
 *
 * Comprehensive settings manager with 9 panels for system configuration
 */

#ifndef APPS_SETTINGS_H
#define APPS_SETTINGS_H

#include <stdint.h>
#include <stdbool.h>
#include "../../gui/compositor/src/compositor.h"
#include "../../gui/widgets/src/widgets.h"

// Settings panels
typedef enum {
    PANEL_DISPLAY = 0,
    PANEL_APPEARANCE = 1,
    PANEL_INPUT = 2,
    PANEL_NETWORK = 3,
    PANEL_SOUND = 4,
    PANEL_POWER = 5,
    PANEL_USERS_SECURITY = 6,
    PANEL_APPLICATIONS = 7,
    PANEL_SYSTEM_UPDATES = 8,
    PANEL_COUNT = 9,
} settings_panel_t;

// Display settings
typedef struct {
    uint32_t resolution_width;
    uint32_t resolution_height;
    uint32_t refresh_rate;
    uint32_t brightness;
    bool night_light_enabled;
    uint32_t night_light_temperature;
    uint32_t scaling;  // 100, 125, 150, 175, 200
    bool multi_monitor_enabled;
    uint32_t monitor_count;
} display_settings_t;

// Appearance settings
typedef struct {
    char theme_name[64];
    bool dark_mode;
    char wallpaper_path[512];
    uint32_t accent_color;
    char font_family[64];
    uint32_t font_size;
    bool transparency_enabled;
    bool animations_enabled;
    uint32_t animation_speed;  // 0-100
} appearance_settings_t;

// Input settings
typedef struct {
    // Keyboard
    uint32_t keyboard_repeat_delay;
    uint32_t keyboard_repeat_rate;
    bool num_lock_on_startup;
    char keyboard_layout[32];
    
    // Mouse
    uint32_t mouse_speed;
    bool mouse_acceleration;
    bool left_handed;
    uint32_t double_click_speed;
    uint32_t scroll_speed;
    
    // Touchpad
    bool touchpad_enabled;
    bool tap_to_click;
    bool natural_scrolling;
    uint32_t touchpad_sensitivity;
} input_settings_t;

// Network settings
typedef struct {
    bool wifi_enabled;
    char connected_ssid[64];
    uint32_t signal_strength;
    bool ethernet_connected;
    char ip_address[64];
    char subnet_mask[64];
    char gateway[64];
    char dns_primary[64];
    char dns_secondary[64];
    bool vpn_enabled;
    char vpn_name[64];
    bool airplane_mode;
} network_settings_t;

// Sound settings
typedef struct {
    uint32_t master_volume;
    uint32_t output_volume;
    uint32_t input_volume;
    bool muted;
    char output_device[128];
    char input_device[128];
    uint32_t balance;  // 0-100 (50 = center)
    bool system_sounds_enabled;
    uint32_t notification_volume;
} sound_settings_t;

// Power settings
typedef struct {
    uint32_t battery_percentage;
    bool charging;
    uint32_t time_remaining;  // minutes
    
    // Power plan
    char power_plan[64];  // "Balanced", "Power Saver", "High Performance"
    uint32_t screen_timeout;  // seconds
    uint32_t sleep_timeout;   // seconds
    bool hibernate_enabled;
    
    // Battery saver
    bool battery_saver_enabled;
    uint32_t battery_saver_threshold;  // Percentage
} power_settings_t;

// User account
typedef struct {
    char username[64];
    char full_name[128];
    char email[128];
    bool is_admin;
    char avatar_path[512];
} user_account_t;

// Users & Security settings
typedef struct {
    user_account_t accounts[16];
    uint32_t account_count;
    uint32_t current_user_index;
    
    bool require_password_on_wake;
    bool auto_login_enabled;
    uint32_t password_timeout;  // minutes
    
    // Security
    bool firewall_enabled;
    bool antivirus_enabled;
    bool secure_boot_enabled;
    bool tpm_enabled;
    uint32_t failed_login_attempts;
} users_security_settings_t;

// Application entry
typedef struct {
    char name[128];
    char path[512];
    bool autostart;
    bool sandboxed;
    uint32_t permissions;  // Bitmask
} app_entry_t;

// Applications settings
typedef struct {
    app_entry_t apps[256];
    uint32_t app_count;
    
    char default_browser[128];
    char default_email[128];
    char default_file_manager[128];
    char default_terminal[128];
    char default_text_editor[128];
    
    bool show_notifications;
    bool notification_sounds;
} applications_settings_t;

// System update
typedef struct {
    char current_version[64];
    char latest_version[64];
    bool update_available;
    uint64_t update_size;  // bytes
    
    bool auto_check_updates;
    bool auto_download_updates;
    bool auto_install_updates;
    uint32_t check_frequency;  // hours
    
    char last_check[64];  // timestamp
    char last_update[64];  // timestamp
} system_update_settings_t;

// Settings context
typedef struct {
    compositor_ctx_t* compositor;
    window_t* settings_window;
    
    // All settings
    display_settings_t display;
    appearance_settings_t appearance;
    input_settings_t input;
    network_settings_t network;
    sound_settings_t sound;
    power_settings_t power;
    users_security_settings_t users_security;
    applications_settings_t applications;
    system_update_settings_t system_update;
    
    // UI state
    settings_panel_t active_panel;
    bool settings_modified;
    
    // Widgets
    widget_t* sidebar;
    widget_t* content_panel;
    widget_t* panel_widgets[PANEL_COUNT];
    
    // Sidebar buttons
    widget_t* btn_display;
    widget_t* btn_appearance;
    widget_t* btn_input;
    widget_t* btn_network;
    widget_t* btn_sound;
    widget_t* btn_power;
    widget_t* btn_users;
    widget_t* btn_apps;
    widget_t* btn_updates;
    
    // Bottom buttons
    widget_t* btn_apply;
    widget_t* btn_reset;
    widget_t* btn_close;
    
    bool running;
} settings_ctx_t;

// Settings initialization
settings_ctx_t* settings_create(compositor_ctx_t* compositor);
void settings_destroy(settings_ctx_t* ctx);
void settings_run(settings_ctx_t* ctx);

// Configuration management
bool settings_load_config(settings_ctx_t* ctx);
bool settings_save_config(settings_ctx_t* ctx);
void settings_apply_changes(settings_ctx_t* ctx);
void settings_reset_to_defaults(settings_ctx_t* ctx);

// Panel switching
void settings_switch_panel(settings_ctx_t* ctx, settings_panel_t panel);

// Display panel
void settings_create_display_panel(settings_ctx_t* ctx);
void settings_update_display_panel(settings_ctx_t* ctx);
void settings_apply_display_settings(settings_ctx_t* ctx);

// Appearance panel
void settings_create_appearance_panel(settings_ctx_t* ctx);
void settings_update_appearance_panel(settings_ctx_t* ctx);
void settings_apply_appearance_settings(settings_ctx_t* ctx);
void settings_preview_theme(settings_ctx_t* ctx, const char* theme_name);

// Input panel
void settings_create_input_panel(settings_ctx_t* ctx);
void settings_update_input_panel(settings_ctx_t* ctx);
void settings_apply_input_settings(settings_ctx_t* ctx);

// Network panel
void settings_create_network_panel(settings_ctx_t* ctx);
void settings_update_network_panel(settings_ctx_t* ctx);
void settings_connect_wifi(settings_ctx_t* ctx, const char* ssid, const char* password);
void settings_disconnect_wifi(settings_ctx_t* ctx);
void settings_configure_vpn(settings_ctx_t* ctx);

// Sound panel
void settings_create_sound_panel(settings_ctx_t* ctx);
void settings_update_sound_panel(settings_ctx_t* ctx);
void settings_apply_sound_settings(settings_ctx_t* ctx);
void settings_test_sound(settings_ctx_t* ctx);

// Power panel
void settings_create_power_panel(settings_ctx_t* ctx);
void settings_update_power_panel(settings_ctx_t* ctx);
void settings_apply_power_settings(settings_ctx_t* ctx);
void settings_set_power_plan(settings_ctx_t* ctx, const char* plan);

// Users & Security panel
void settings_create_users_panel(settings_ctx_t* ctx);
void settings_update_users_panel(settings_ctx_t* ctx);
void settings_add_user(settings_ctx_t* ctx, const char* username, const char* password);
void settings_remove_user(settings_ctx_t* ctx, uint32_t user_index);
void settings_change_password(settings_ctx_t* ctx, const char* old_pass, const char* new_pass);

// Applications panel
void settings_create_applications_panel(settings_ctx_t* ctx);
void settings_update_applications_panel(settings_ctx_t* ctx);
void settings_set_default_app(settings_ctx_t* ctx, const char* category, const char* app_path);
void settings_toggle_app_autostart(settings_ctx_t* ctx, uint32_t app_index);

// System Updates panel
void settings_create_updates_panel(settings_ctx_t* ctx);
void settings_update_updates_panel(settings_ctx_t* ctx);
void settings_check_for_updates(settings_ctx_t* ctx);
void settings_download_updates(settings_ctx_t* ctx);
void settings_install_updates(settings_ctx_t* ctx);

// Rendering
void settings_render(settings_ctx_t* ctx);
void settings_render_sidebar(settings_ctx_t* ctx);
void settings_render_panel(settings_ctx_t* ctx);

// Input handling
void settings_handle_key(settings_ctx_t* ctx, uint32_t keycode, uint32_t modifiers, bool pressed);
void settings_handle_mouse(settings_ctx_t* ctx, int32_t x, int32_t y, uint32_t buttons);

#endif // APPS_SETTINGS_H
