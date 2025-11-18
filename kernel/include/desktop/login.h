/**
 * @file login.h
 * @brief Login screen interface
 */

#ifndef KERNEL_DESKTOP_LOGIN_H
#define KERNEL_DESKTOP_LOGIN_H

#include "../types.h"
#include "../errors.h"

// Login screen state
typedef struct {
    char username[64];
    char password[128];
    bool show_password;
    bool logged_in;
    void* username_widget;
    void* password_widget;
    void* login_button_widget;
    void* create_user_button_widget;
    bool initialized;
} login_screen_t;

// Login functions
error_code_t login_screen_init(void);
error_code_t login_screen_show(void);
error_code_t login_screen_hide(void);
error_code_t login_screen_render(void);
error_code_t login_screen_handle_input(void);
bool login_screen_is_logged_in(void);
login_screen_t* login_screen_get(void);

#endif // KERNEL_DESKTOP_LOGIN_H

