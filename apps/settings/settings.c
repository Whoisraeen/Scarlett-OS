/**
 * @file settings.c
 * @brief System Settings Implementation
 */

#include "settings.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../libs/libc/include/syscall.h" // For syscall wrappers
#include "../../libs/libgui/include/compositor_ipc.h" // For compositor_get_screen_info

// Syscall wrappers
static int sys_open(const char* path, int flags) {
    return (int)syscall(SYS_OPEN, (uint64_t)path, (uint64_t)flags, 0, 0, 0);
}

static int sys_close(int fd) {
    return (int)syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
}

static long sys_read(int fd, void* buf, size_t count) {
    return (long)syscall(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count, 0, 0);
}

static long sys_write(int fd, const void* buf, size_t count) {
    return (long)syscall(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, count, 0, 0);
}

static void sys_yield(void) {
    syscall(SYS_YIELD, 0, 0, 0, 0, 0);
}

static uint64_t sys_ipc_create_port(void) {
    return syscall(SYS_IPC_CREATE_PORT, 0, 0, 0, 0, 0);
}

static void sys_set_process_ipc_port(uint64_t port) {
    syscall(SYS_SET_PROCESS_IPC_PORT, port, 0, 0, 0, 0);
}

static int sys_ipc_receive(uint64_t port, ipc_message_t* msg) {
    return (int)syscall(SYS_IPC_RECEIVE, port, (uint64_t)msg, 0, 0, 0);
}


// O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC (from fcntl.h typically)
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

// Global context for callbacks
static settings_ctx_t* g_settings_ctx = NULL;


// ============================================================================ 
// Settings Initialization
// ============================================================================ 

settings_ctx_t* settings_create(compositor_ctx_t* compositor) {
    settings_ctx_t* ctx = (settings_ctx_t*)calloc(1, sizeof(settings_ctx_t));
    if (!ctx) return NULL;

    ctx->compositor = compositor;
    g_settings_ctx = ctx; // Set global context

    // Query screen dimensions from compositor
    uint32_t screen_width = 1920; // Default if compositor not ready
    uint32_t screen_height = 1080;
    compositor_get_screen_info(&screen_width, &screen_height);
    
    // Create window
    uint32_t width = 900;
    uint32_t height = 600;
    int32_t x = (screen_width - width) / 2;
    int32_t y = (screen_height - height) / 2;
    
    ctx->settings_window = window_create("Settings", width, height);
    if (!ctx->settings_window) {
        free(ctx);
        return NULL;
    }
    
    // Create root panel
    widget_t* root = panel_create();
    widget_set_size(root, width, height);
    widget_set_colors(root, 0xFF000000, 0xFFECF0F1);
    ctx->settings_window->root = root;

    // Load configuration
    settings_load_config(ctx);
    
    // Create all panels
    settings_create_display_panel(ctx);
    settings_create_appearance_panel(ctx);
    settings_create_input_panel(ctx);
    settings_create_network_panel(ctx);
    settings_create_sound_panel(ctx);
    settings_create_power_panel(ctx);
    settings_create_users_panel(ctx);
    settings_create_applications_panel(ctx);
    settings_create_updates_panel(ctx);
    
    // Start with display panel
    ctx->active_panel = PANEL_DISPLAY;
    ctx->settings_modified = false;
    ctx->running = true;
    
    return ctx;
}

void settings_destroy(settings_ctx_t* ctx) {
    if (!ctx) return;
    if (ctx->settings_modified) {
        settings_save_config(ctx);
    }
    if (ctx->settings_window) {
        window_destroy(ctx->settings_window);
    }
    free(ctx);
}

// ============================================================================ 
// Configuration Management
// ============================================================================ 

bool settings_load_config(settings_ctx_t* ctx) {
    // Initialize with defaults (existing code)
    // Display
    ctx->display.resolution_width = 1920; ctx->display.resolution_height = 1080; ctx->display.refresh_rate = 60; ctx->display.brightness = 80; ctx->display.night_light_enabled = false; ctx->display.night_light_temperature = 3400; ctx->display.scaling = 100; ctx->display.multi_monitor_enabled = false; ctx->display.monitor_count = 1;
    // Appearance
    strcpy(ctx->appearance.theme_name, "Dark"); ctx->appearance.dark_mode = true; strcpy(ctx->appearance.wallpaper_path, "/usr/share/wallpapers/default.jpg"); ctx->appearance.accent_color = 0xFF0078D4; strcpy(ctx->appearance.font_family, "Inter"); ctx->appearance.font_size = 11; ctx->appearance.transparency_enabled = true; ctx->appearance.animations_enabled = true; ctx->appearance.animation_speed = 75;
    // Input - Keyboard
    ctx->input.keyboard_repeat_delay = 500; ctx->input.keyboard_repeat_rate = 30; ctx->input.num_lock_on_startup = true; strcpy(ctx->input.keyboard_layout, "US");
    // Input - Mouse
    ctx->input.mouse_speed = 50; ctx->input.mouse_acceleration = true; ctx->input.left_handed = false; ctx->input.double_click_speed = 500; ctx->input.scroll_speed = 3;
    // Input - Touchpad
    ctx->input.touchpad_enabled = true; ctx->input.tap_to_click = true; ctx->input.natural_scrolling = true; ctx->input.touchpad_sensitivity = 50;
    // Network
    ctx->network.wifi_enabled = true; strcpy(ctx->network.connected_ssid, ""); ctx->network.signal_strength = 0; ctx->network.ethernet_connected = false; strcpy(ctx->network.ip_address, "0.0.0.0"); strcpy(ctx->network.subnet_mask, "255.255.255.0"); strcpy(ctx->network.gateway, "192.168.1.1"); strcpy(ctx->network.dns_primary, "8.8.8.8"); strcpy(ctx->network.dns_secondary, "8.8.4.4"); ctx->network.vpn_enabled = false; strcpy(ctx->network.vpn_name, ""); ctx->network.airplane_mode = false;
    // Sound
    ctx->sound.master_volume = 75; ctx->sound.output_volume = 75; ctx->sound.input_volume = 50; ctx->sound.muted = false; strcpy(ctx->sound.output_device, "Default Output"); strcpy(ctx->sound.input_device, "Default Input"); ctx->sound.balance = 50; ctx->sound.system_sounds_enabled = true; ctx->sound.notification_volume = 60;
    // Power
    ctx->power.battery_percentage = 100; ctx->power.charging = true; ctx->power.time_remaining = 0; strcpy(ctx->power.power_plan, "Balanced"); ctx->power.screen_timeout = 300; ctx->power.sleep_timeout = 900; ctx->power.hibernate_enabled = true; ctx->power.battery_saver_enabled = false; ctx->power.battery_saver_threshold = 20;
    // Users & Security
    ctx->users_security.account_count = 1; strcpy(ctx->users_security.accounts[0].username, "user"); strcpy(ctx->users_security.accounts[0].full_name, "Default User"); strcpy(ctx->users_security.accounts[0].email, "user@localhost"); ctx->users_security.accounts[0].is_admin = true; strcpy(ctx->users_security.accounts[0].avatar_path, ""); ctx->users_security.current_user_index = 0; ctx->users_security.require_password_on_wake = true; ctx->users_security.auto_login_enabled = false; ctx->users_security.password_timeout = 5; ctx->users_security.firewall_enabled = true; ctx->users_security.antivirus_enabled = true; ctx->users_security.secure_boot_enabled = true; ctx->users_security.tpm_enabled = true; ctx->users_security.failed_login_attempts = 0;
    // Applications
    ctx->applications.app_count = 0; strcpy(ctx->applications.default_browser, "Browser"); strcpy(ctx->applications.default_email, "Mail"); strcpy(ctx->applications.default_file_manager, "File Manager"); strcpy(ctx->applications.default_terminal, "Terminal"); strcpy(ctx->applications.default_text_editor, "Text Editor"); ctx->applications.show_notifications = true; ctx->applications.notification_sounds = true;
    // System Updates
    strcpy(ctx->system_update.current_version, "0.1.0"); strcpy(ctx->system_update.latest_version, "0.1.0"); ctx->system_update.update_available = false; ctx->system_update.update_size = 0; ctx->system_update.auto_check_updates = true; ctx->system_update.auto_download_updates = false; ctx->system_update.auto_install_updates = false; ctx->system_update.check_frequency = 24; strcpy(ctx->system_update.last_check, "Never"); strcpy(ctx->system_update.last_update, "Never");
    
    // Load from actual config file
    int fd = sys_open("/etc/settings.conf", O_RDONLY);
    if (fd >= 0) {
        sys_read(fd, ctx, sizeof(settings_ctx_t)); // Read entire context for simplicity
        sys_close(fd);
        printf("Loaded settings from /etc/settings.conf\n");
        return true;
    }
    printf("Settings file /etc/settings.conf not found. Using defaults.\n");
    return false;
}

bool settings_save_config(settings_ctx_t* ctx) {
    // Save to config file
    printf("Saving settings configuration...\n");
    int fd = sys_open("/etc/settings.conf", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd >= 0) {
        sys_write(fd, ctx, sizeof(settings_ctx_t)); // Write entire context for simplicity
        sys_close(fd);
        printf("Saved settings to /etc/settings.conf\n");
        ctx->settings_modified = false;
        return true;
    }
    printf("Failed to save settings to /etc/settings.conf\n");
    return false;
}

void settings_apply_changes(settings_ctx_t* ctx) {
    switch (ctx->active_panel) {
        case PANEL_DISPLAY:
            settings_apply_display_settings(ctx);
            break;
        case PANEL_APPEARANCE:
            settings_apply_appearance_settings(ctx);
            break;
        case PANEL_INPUT:
            settings_apply_input_settings(ctx);
            break;
        case PANEL_SOUND:
            settings_apply_sound_settings(ctx);
            break;
        case PANEL_POWER:
            settings_apply_power_settings(ctx);
            break;
        default:
            break;
    }
    
    settings_save_config(ctx);
}

void settings_reset_to_defaults(settings_ctx_t* ctx) {
    settings_load_config(ctx); // Reload defaults
    settings_update_display_panel(ctx);
    settings_update_appearance_panel(ctx);
    settings_update_input_panel(ctx);
    settings_update_network_panel(ctx);
    settings_update_sound_panel(ctx);
    settings_update_power_panel(ctx);
    settings_update_users_panel(ctx);
    settings_update_applications_panel(ctx);
    settings_update_updates_panel(ctx);
}

// ============================================================================ 
// Panel Management
// ============================================================================ 

void settings_switch_panel(settings_ctx_t* ctx, settings_panel_t panel) {
    if (panel < PANEL_COUNT) {
        ctx->active_panel = panel;
    }
}

// ============================================================================ 
// Display Panel
// ============================================================================ 

void settings_create_display_panel(settings_ctx_t* ctx) {
    printf("Created display panel\n");
}

void settings_update_display_panel(settings_ctx_t* ctx) {
    printf("Updated display panel\n");
}

void settings_apply_display_settings(settings_ctx_t* ctx) {
    printf("Applying display settings: %ux%u @ %uHz\n",
           ctx->display.resolution_width,
           ctx->display.resolution_height,
           ctx->display.refresh_rate);
}

// ============================================================================ 
// Appearance Panel
// ============================================================================ 

void settings_create_appearance_panel(settings_ctx_t* ctx) {
    printf("Created appearance panel\n");
}

void settings_update_appearance_panel(settings_ctx_t* ctx) {
    printf("Updated appearance panel\n");
}

void settings_apply_appearance_settings(settings_ctx_t* ctx) {
    printf("Applying appearance settings: Theme=%s, Dark=%d\n",
           ctx->appearance.theme_name,
           ctx->appearance.dark_mode);
}

void settings_preview_theme(settings_ctx_t* ctx, const char* theme_name) {
    printf("Previewing theme: %s\n", theme_name);
}

// ============================================================================ 
// Input Panel
// ============================================================================ 

void settings_create_input_panel(settings_ctx_t* ctx) {
    printf("Created input panel\n");
}

void settings_update_input_panel(settings_ctx_t* ctx) {
    printf("Updated input panel\n");
}

void settings_apply_input_settings(settings_ctx_t* ctx) {
    printf("Applying input settings\n");
}

// ============================================================================ 
// Network Panel
// ============================================================================ 

void settings_create_network_panel(settings_ctx_t* ctx) {
    printf("Created network panel\n");
}

void settings_update_network_panel(settings_ctx_t* ctx) {
    printf("Updated network panel\n");
}

void settings_connect_wifi(settings_ctx_t* ctx, const char* ssid, const char* password) {
    printf("Connecting to WiFi: %s\n", ssid);
    // Placeholder: In a real system, send IPC to network service
    // Assume success for now
    ctx->network.wifi_enabled = true;
    strncpy(ctx->network.connected_ssid, ssid, sizeof(ctx->network.connected_ssid) - 1);
    ctx->network.signal_strength = 90; // Dummy
    ctx->settings_modified = true;
}

void settings_disconnect_wifi(settings_ctx_t* ctx) {
    printf("Disconnecting WiFi\n");
    ctx->network.connected_ssid[0] = '\0';
    ctx->network.signal_strength = 0;
    ctx->settings_modified = true;
}

void settings_configure_vpn(settings_ctx_t* ctx) {
    printf("Configuring VPN\n");
    // Placeholder: Send IPC to network service
    ctx->settings_modified = true;
}

// ============================================================================ 
// Sound Panel
// ============================================================================ 

void settings_create_sound_panel(settings_ctx_t* ctx) {
    printf("Created sound panel\n");
}

void settings_update_sound_panel(settings_ctx_t* ctx) {
    printf("Updated sound panel\n");
}

void settings_apply_sound_settings(settings_ctx_t* ctx) {
    printf("Applying sound settings: Volume=%u%%\n", ctx->sound.master_volume);
    // Placeholder: Send IPC to audio service
}

void settings_test_sound(settings_ctx_t* ctx) {
    printf("Playing test sound\n");
    // Placeholder: Send IPC to audio service
}

// ============================================================================ 
// Power Panel
// ============================================================================ 

void settings_create_power_panel(settings_ctx_t* ctx) {
    printf("Created power panel\n");
}

void settings_update_power_panel(settings_ctx_t* ctx) {
    printf("Updated power panel\n");
}

void settings_apply_power_settings(settings_ctx_t* ctx) {
    printf("Applying power settings: Plan=%s\n", ctx->power.power_plan);
    // Placeholder: Send IPC to power management service
}

void settings_set_power_plan(settings_ctx_t* ctx, const char* plan) {
    strncpy(ctx->power.power_plan, plan, sizeof(ctx->power.power_plan) - 1);
    ctx->settings_modified = true;
}

// ============================================================================ 
// Users & Security Panel
// ============================================================================ 

void settings_create_users_panel(settings_ctx_t* ctx) {
    printf("Created users & security panel\n");
}

void settings_update_users_panel(settings_ctx_t* ctx) {
    printf("Updated users & security panel\n");
}

void settings_add_user(settings_ctx_t* ctx, const char* username, const char* password) {
    if (ctx->users_security.account_count < 16) {
        user_account_t* user = &ctx->users_security.accounts[ctx->users_security.account_count];
        strncpy(user->username, username, sizeof(user->username) - 1);
        user->is_admin = false;
        ctx->users_security.account_count++;
        ctx->settings_modified = true;
        printf("Added user: %s\n", username);
        // Placeholder: Send IPC to user management service
    }
}

void settings_remove_user(settings_ctx_t* ctx, uint32_t user_index) {
    if (user_index < ctx->users_security.account_count && user_index != ctx->users_security.current_user_index) {
        // Shift users down
        for (uint32_t i = user_index; i < ctx->users_security.account_count - 1; i++) {
            ctx->users_security.accounts[i] = ctx->users_security.accounts[i + 1];
        }
        ctx->users_security.account_count--;
        ctx->settings_modified = true;
        printf("Removed user at index %u\n", user_index);
        // Placeholder: Send IPC to user management service
    }
}

void settings_change_password(settings_ctx_t* ctx, const char* old_pass, const char* new_pass) {
    printf("Changing password\n");
    // Placeholder: Send IPC to authentication service
    ctx->settings_modified = true;
}

// ============================================================================ 
// Applications Panel
// ============================================================================ 

void settings_create_applications_panel(settings_ctx_t* ctx) {
    printf("Created applications panel\n");
}

void settings_update_applications_panel(settings_ctx_t* ctx) {
    printf("Updated applications panel\n");
}

void settings_set_default_app(settings_ctx_t* ctx, const char* category, const char* app_path) {
    if (strcmp(category, "browser") == 0) {
        strncpy(ctx->applications.default_browser, app_path, sizeof(ctx->applications.default_browser) - 1);
    } else if (strcmp(category, "email") == 0) {
        strncpy(ctx->applications.default_email, app_path, sizeof(ctx->applications.default_email) - 1);
    } else if (strcmp(category, "file_manager") == 0) {
        strncpy(ctx->applications.default_file_manager, app_path, sizeof(ctx->applications.default_file_manager) - 1);
    } else if (strcmp(category, "terminal") == 0) {
        strncpy(ctx->applications.default_terminal, app_path, sizeof(ctx->applications.default_terminal) - 1);
    } else if (strcmp(category, "text_editor") == 0) {
        strncpy(ctx->applications.default_text_editor, app_path, sizeof(ctx->applications.default_text_editor) - 1);
    }
    ctx->settings_modified = true;
}

void settings_toggle_app_autostart(settings_ctx_t* ctx, uint32_t app_index) {
    // This assumes app_index maps to a valid app in ctx->applications.apps
    // Placeholder for actual app autostart toggle
    printf("Toggling autostart for app index %u\n", app_index);
    ctx->settings_modified = true;
}

// ============================================================================ 
// System Updates Panel
// ============================================================================ 

void settings_create_updates_panel(settings_ctx_t* ctx) {
    printf("Created system updates panel\n");
}

void settings_update_updates_panel(settings_ctx_t* ctx) {
    printf("Updated system updates panel\n");
}

void settings_check_for_updates(settings_ctx_t* ctx) {
    printf("Checking for updates...\n");
    // Placeholder: Send IPC to update service
    strcpy(ctx->system_update.last_check, "Just now");
    ctx->system_update.update_available = false; // Assume no update for now
    ctx->settings_modified = true;
}

void settings_download_updates(settings_ctx_t* ctx) {
    printf("Downloading updates...\n");
    // Placeholder: Send IPC to update service
    ctx->settings_modified = true;
}

void settings_install_updates(settings_ctx_t* ctx) {
    printf("Installing updates...\n");
    // Placeholder: Send IPC to update service, reboot
    ctx->settings_modified = true;
}

// ============================================================================ 
// Rendering
// ============================================================================ 

void settings_render(settings_ctx_t* ctx) {
    if (!ctx) return;
    window_render(ctx->settings_window); // Trigger compositor to render
}

void settings_render_sidebar(settings_ctx_t* ctx) {
    // Render sidebar with panel buttons
    // Widget rendering should handle this implicitly
}

void settings_render_panel(settings_ctx_t* ctx) {
    // Render active panel
    // Widget rendering should handle this implicitly
}

// ============================================================================ 
// Input Handling
// ============================================================================ 

void settings_handle_key(settings_ctx_t* ctx, uint32_t keycode, uint32_t modifiers, bool pressed) {
    // Handle keyboard input (e.g., Tab to switch focus, Enter to activate button)
    // For now, no specific key handling other than what widgets do
}

void settings_handle_mouse(settings_ctx_t* ctx, int32_t x, int32_t y, uint32_t buttons) {
    // Handle mouse input (e.g., passing to focused widget)
}

// ============================================================================ 
// Main Loop
// ============================================================================ 

void settings_run(settings_ctx_t* ctx) {
    if (!ctx) return;
    g_settings_ctx = ctx; // Set global context

    // Create and register IPC port for settings service
    uint64_t settings_port_id = sys_ipc_create_port();
    if (settings_port_id == 0) {
        printf("Failed to create settings IPC port\n");
        return;
    }
    sys_set_process_ipc_port(settings_port_id);
    printf("Settings running on port %lu...\n", settings_port_id);

    // Show settings window
    window_show(ctx->settings_window);

    ipc_message_t msg;

    while (ctx->running) {
        // Process IPC messages (e.g., from compositor for input events)
        if (sys_ipc_receive(settings_port_id, &msg) == 0) {
            // Handle compositor input events
            if (msg.msg_id == 100) { // MOUSE_BUTTON_EVENT
                // int32_t x = *(int32_t*)&msg.inline_data[2];
                // int32_t y = *(int32_t*)&msg.inline_data[6];
                // uint32_t button = *(uint32_t*)&msg.inline_data[0];
                // bool pressed = (bool)msg.inline_data[4];
                // settings_handle_mouse(ctx, x, y, button, pressed);
            } else if (msg.msg_id == 101) { // KEYBOARD_EVENT
                // uint32_t keycode = *(uint32_t*)&msg.inline_data[0];
                // bool pressed = (bool)msg.inline_data[4];
                // settings_handle_key(ctx, keycode, 0, pressed);
            }
        }
        
        settings_render(ctx); // Render only if needed
        sys_yield(); // Yield CPU
    }
    printf("Settings loop finished.\n");
}