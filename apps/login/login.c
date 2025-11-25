/**
 * @file login.c
 * @brief Login Screen Implementation
 */

#include "login.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../libs/libc/include/syscall.h"
#include "../../libs/libgui/include/compositor_ipc.h"

// Global login context for callbacks
static login_ctx_t* g_login_ctx = NULL;

// Syscall wrappers
static uint64_t sys_ipc_create_port(void) {
    return syscall(SYS_IPC_CREATE_PORT, 0, 0, 0, 0, 0);
}

static void sys_set_process_ipc_port(uint64_t port) {
    syscall(SYS_SET_PROCESS_IPC_PORT, port, 0, 0, 0, 0);
}

static int sys_ipc_receive(uint64_t port, ipc_message_t* msg) {
    return (int)syscall(SYS_IPC_RECEIVE, port, (uint64_t)msg, 0, 0, 0);
}

static int sys_ipc_send(uint64_t port, ipc_message_t* msg) {
    return (int)syscall(SYS_IPC_SEND, port, (uint64_t)msg, 0, 0, 0);
}

static void sys_yield(void) {
    syscall(SYS_YIELD, 0, 0, 0, 0, 0);
}

// IPC message IDs for Authentication Service (arbitrary for now)
#define AUTH_SVC_PORT_NAME "auth_service" // Name if using a name service
#define AUTH_MSG_AUTHENTICATE 1
#define AUTH_MSG_CREATE_USER 2

// Create login screen
login_ctx_t* login_create(compositor_ctx_t* compositor) {
    login_ctx_t* ctx = (login_ctx_t*)malloc(sizeof(login_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(login_ctx_t));
    g_login_ctx = ctx; // Set global context

    ctx->compositor = compositor;
    ctx->show_password = false;
    ctx->logged_in = false;

    // Query screen dimensions from compositor
    uint32_t screen_width = 1920; // Default if compositor not ready
    uint32_t screen_height = 1080;
    compositor_get_screen_info(&screen_width, &screen_height);

    // Create login window (centered)
    uint32_t width = 400;
    uint32_t height = 300;
    int32_t x = (screen_width - width) / 2;
    int32_t y = (screen_height - height) / 2;

    ctx->login_window = window_create("Login", width, height);
    if (!ctx->login_window) {
        free(ctx);
        return NULL;
    }

    // Create root panel
    widget_t* root = panel_create();
    widget_set_size(root, width, height);
    widget_set_colors(root, 0xFF000000, 0xFF34495E); // Dark blue background
    ctx->login_window->root = root;

    // Username input
    widget_t* user_label = label_create("Username:");
    widget_set_position(user_label, 50, 50);
    widget_set_colors(user_label, 0xFFFFFFFF, 0x00000000);
    widget_add_child(root, user_label);

    ctx->username_input = text_input_create();
    widget_set_position(ctx->username_input, 50, 70);
    widget_set_size(ctx->username_input, 300, 30);
    text_input_set_placeholder(ctx->username_input, "Enter username");
    widget_add_child(root, ctx->username_input);

    // Password input
    widget_t* pass_label = label_create("Password:");
    widget_set_position(pass_label, 50, 110);
    widget_set_colors(pass_label, 0xFFFFFFFF, 0x00000000);
    widget_add_child(root, pass_label);

    ctx->password_input = text_input_create();
    widget_set_position(ctx->password_input, 50, 130);
    widget_set_size(ctx->password_input, 300, 30);
    text_input_set_placeholder(ctx->password_input, "Enter password");
    // text_input_set_password_mode(ctx->password_input, true); // Assuming this widget function exists
    widget_add_child(root, ctx->password_input);

    // Login button
    ctx->login_button = button_create("Login");
    widget_set_position(ctx->login_button, 150, 190);
    widget_set_size(ctx->login_button, 100, 40);
    widget_set_click_handler(ctx->login_button, (event_callback_t)login_button_clicked, (void*)ctx);
    widget_add_child(root, ctx->login_button);
    
    // Create user button
    ctx->create_user_button = button_create("Create User");
    widget_set_position(ctx->create_user_button, 130, 240);
    widget_set_size(ctx->create_user_button, 140, 30);
    widget_set_click_handler(ctx->create_user_button, (event_callback_t)create_user_button_clicked, (void*)ctx);
    widget_add_child(root, ctx->create_user_button);

    ctx->running = true;

    return ctx;
}

// Destroy login screen
void login_destroy(login_ctx_t* ctx) {
    if (!ctx) return;

    if (ctx->login_window) {
        window_destroy(ctx->login_window);
    }

    free(ctx);
}

// Show login window
void login_show(login_ctx_t* ctx) {
    if (!ctx) return;
    window_show(ctx->login_window);
}

// Hide login window
void login_hide(login_ctx_t* ctx) {
    if (!ctx) return;
    window_hide(ctx->login_window);
}

// Authenticate user via IPC to an auth service
bool login_authenticate(login_ctx_t* ctx, const char* username, const char* password) {
    // In a real system, would find auth service port and send IPC message
    // For now, simple hardcoded check
    if (strcmp(username, "user") == 0 && strcmp(password, "password") == 0) {
        return true;
    }
    return false;
}

// Render login screen
void login_render(login_ctx_t* ctx) {
    if (!ctx) return;
    window_render(ctx->login_window);
}

// Event handler for login button
void login_button_clicked(widget_t* widget, void* userdata) {
    login_ctx_t* ctx = (login_ctx_t*)userdata;
    if (!ctx) return;

    const char* username = text_input_get_text(ctx->username_input);
    const char* password = text_input_get_text(ctx->password_input);

    if (login_authenticate(ctx, username, password)) {
        printf("Login successful for user: %s\n", username);
        ctx->logged_in = true;
        ctx->running = false; // Exit login screen loop
        login_hide(ctx);
    } else {
        printf("Login failed for user: %s\n", username);
        // Display error message (e.g., in a label widget)
    }
}

// Event handler for create user button
void create_user_button_clicked(widget_t* widget, void* userdata) {
    login_ctx_t* ctx = (login_ctx_t*)userdata;
    if (!ctx) return;

    // Call authentication service via IPC to create user
    printf("Create User clicked (not implemented)\n");
}

// Input handling for keyboard
void login_handle_key(login_ctx_t* ctx, uint32_t keycode, bool pressed) {
    if (!ctx || !pressed) return;

    // Simulate sending key events to focused input widgets
    if (ctx->username_input && widget_is_focused(ctx->username_input)) { // Assume widget_is_focused exists
        text_input_handle_key(ctx->username_input, keycode); // Assume text_input_handle_key exists
    } else if (ctx->password_input && widget_is_focused(ctx->password_input)) {
        text_input_handle_key(ctx->password_input, keycode);
    }
    
    // Press Enter to login
    if (keycode == 0x0D) { // Enter key
        login_button_clicked(ctx->login_button, (void*)ctx);
    }
}

// Input handling for mouse button
void login_handle_mouse_button(login_ctx_t* ctx, int32_t x, int32_t y, uint32_t button, bool pressed) {
    if (!ctx || !pressed) return;

    // Simulate passing mouse events to widgets for focus/clicks
    if (widget_handle_mouse_button(ctx->login_button, x, y, pressed)) return; // Pass to login button
    if (widget_handle_mouse_button(ctx->create_user_button, x, y, pressed)) return;
    if (widget_handle_mouse_button(ctx->username_input, x, y, pressed)) return;
    if (widget_handle_mouse_button(ctx->password_input, x, y, pressed)) return;
}

// Main login loop
void login_run(login_ctx_t* ctx) {
    if (!ctx) return;
    g_login_ctx = ctx; // Set global context

    // Create and register IPC port for login service
    uint64_t login_port_id = sys_ipc_create_port();
    if (login_port_id == 0) {
        printf("Failed to create login IPC port\n");
        return;
    }
    sys_set_process_ipc_port(login_port_id);
    printf("Login running on port %lu...\n", login_port_id);

    login_show(ctx); // Show login window

    ipc_message_t msg;

    while (ctx->running) {
        // Process IPC messages from compositor (input events)
        if (sys_ipc_receive(login_port_id, &msg) == 0) {
            // Check for compositor input events
            if (msg.msg_id == 100) { // MOUSE_BUTTON_EVENT
                // int32_t x = *(int32_t*)&msg.inline_data[2]; // X from compositor is relative to window
                // int32_t y = *(int32_t*)&msg.inline_data[6];
                // uint32_t button = *(uint32_t*)&msg.inline_data[0];
                // bool pressed = (bool)msg.inline_data[4];
                // login_handle_mouse_button(ctx, x, y, button, pressed);
                // For now, simplified event routing from compositor is not widget specific, just general
            } else if (msg.msg_id == 101) { // KEYBOARD_EVENT
                uint32_t keycode = *(uint32_t*)&msg.inline_data[0];
                bool pressed = (bool)msg.inline_data[4];
                login_handle_key(ctx, keycode, pressed);
            }
        }

        login_render(ctx); // Render only if needed
        sys_yield();
    }
    printf("Login loop finished.\n");
}