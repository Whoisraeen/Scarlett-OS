/**
 * @file login.h
 * @brief Login Screen for Scarlett OS
 *
 * User-space login screen application
 */

#ifndef APPS_LOGIN_H
#define APPS_LOGIN_H

#include <stdint.h>
#include <stdbool.h>
#include "../../gui/compositor/src/compositor.h"
#include "../../gui/widgets/src/widgets.h"

// Login screen context
typedef struct {
    compositor_ctx_t* compositor;
    window_t* login_window;
    
    char username[64];
    char password[128];
    bool show_password;
    bool logged_in;
    
    // Widgets
    widget_t* username_input;
    widget_t* password_input;
    widget_t* login_button;
    widget_t* create_user_button;
    
    bool running;
} login_ctx_t;

// Login initialization
login_ctx_t* login_create(compositor_ctx_t* compositor);
void login_destroy(login_ctx_t* ctx);
void login_run(login_ctx_t* ctx);

// Authentication
bool login_authenticate(login_ctx_t* ctx, const char* username, const char* password);
void login_create_user(login_ctx_t* ctx, const char* username, const char* password);

// Display
void login_show(login_ctx_t* ctx);
void login_hide(login_ctx_t* ctx);
void login_render(login_ctx_t* ctx);

// Input handling
void login_handle_key(login_ctx_t* ctx, uint32_t keycode, bool pressed);
void login_handle_mouse_button(login_ctx_t* ctx, int32_t x, int32_t y, uint32_t button, bool pressed);

#endif // APPS_LOGIN_H

