/**
 * @file login.c
 * @brief Login Screen Implementation
 *
 * User-space login screen application
 */

#include "login.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Create login screen
login_ctx_t* login_create(compositor_ctx_t* compositor) {
    login_ctx_t* ctx = (login_ctx_t*)malloc(sizeof(login_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(login_ctx_t));

    ctx->compositor = compositor;

    // Create fullscreen login window
    ctx->login_window = window_create("Login", compositor->screen_width, compositor->screen_height);
    if (!ctx->login_window) {
        free(ctx);
        return NULL;
    }

    // Create widgets
    ctx->username_input = textbox_create();
    widget_set_position(ctx->username_input, compositor->screen_width / 2 - 150, compositor->screen_height / 2 - 60);
    widget_set_size(ctx->username_input, 300, 32);

    ctx->password_input = textbox_create();
    widget_set_position(ctx->password_input, compositor->screen_width / 2 - 150, compositor->screen_height / 2);
    widget_set_size(ctx->password_input, 300, 32);
    textbox_set_password(ctx->password_input, true);

    ctx->login_button = button_create("Login");
    widget_set_position(ctx->login_button, compositor->screen_width / 2 - 150, compositor->screen_height / 2 + 60);
    widget_set_size(ctx->login_button, 120, 36);

    ctx->create_user_button = button_create("Create User");
    widget_set_position(ctx->create_user_button, compositor->screen_width / 2 + 30, compositor->screen_height / 2 + 60);
    widget_set_size(ctx->create_user_button, 120, 36);

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

// Authenticate user
bool login_authenticate(login_ctx_t* ctx, const char* username, const char* password) {
    if (!ctx || !username || !password) return false;

    // TODO: Call authentication service via IPC
    // For now, return true for any non-empty credentials
    return (strlen(username) > 0 && strlen(password) > 0);
}

// Create new user
void login_create_user(login_ctx_t* ctx, const char* username, const char* password) {
    if (!ctx || !username || !password) return;

    // TODO: Call user management service via IPC
    printf("Creating user: %s\n", username);
}

// Show login screen
void login_show(login_ctx_t* ctx) {
    if (!ctx) return;
    ctx->logged_in = false;
    // TODO: Show window
}

// Hide login screen
void login_hide(login_ctx_t* ctx) {
    if (!ctx) return;
    memset(ctx->username, 0, sizeof(ctx->username));
    memset(ctx->password, 0, sizeof(ctx->password));
    // TODO: Hide window
}

// Render login screen
void login_render(login_ctx_t* ctx) {
    if (!ctx || !ctx->login_window) return;

    void* canvas = ctx->login_window->framebuffer;
    uint32_t width = ctx->login_window->width;
    uint32_t height = ctx->login_window->height;

    // Draw gradient background
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t t_num = (y * 256) / height;
            uint8_t r = (uint8_t)(20 + ((60 - 20) * t_num) / 256);
            uint8_t g = (uint8_t)(25 + ((40 - 25) * t_num) / 256);
            uint8_t b = (uint8_t)(50 + ((100 - 50) * t_num) / 256);
            ((uint32_t*)canvas)[y * width + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }

    // Render widgets
    if (ctx->username_input) {
        widget_paint(ctx->username_input, canvas);
    }
    if (ctx->password_input) {
        widget_paint(ctx->password_input, canvas);
    }
    if (ctx->login_button) {
        widget_paint(ctx->login_button, canvas);
    }
    if (ctx->create_user_button) {
        widget_paint(ctx->create_user_button, canvas);
    }

    window_render(ctx->login_window);
}

// Handle keyboard input
void login_handle_key(login_ctx_t* ctx, uint32_t keycode, bool pressed) {
    if (!ctx || !pressed) return;

    // TODO: Handle keyboard input for username/password fields
}

// Handle mouse input
void login_handle_mouse_button(login_ctx_t* ctx, int32_t x, int32_t y, uint32_t button, bool pressed) {
    if (!ctx || !pressed) return;

    // TODO: Handle button clicks
    if (button == 1) {  // Left click
        // Check if login button clicked
        // Check if create user button clicked
    }
}

// Main login loop
void login_run(login_ctx_t* ctx) {
    if (!ctx) return;

    login_show(ctx);

    while (ctx->running) {
        // TODO: Process IPC messages from compositor
        // TODO: Handle input events

        // Render
        login_render(ctx);

        // TODO: Yield CPU or sleep
    }
}

