/**
 * @file login.c
 * @brief Login screen implementation
 */

#include "../include/desktop/login.h"
#include "../include/ui/widget.h"
#include "../include/ui/theme.h"
#include "../include/graphics/graphics.h"
#include "../include/graphics/framebuffer.h"
#include "../include/auth/user.h"
#include "../include/auth/password_hash.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"

// Login screen state
static login_screen_t login_state = {0};

/**
 * Initialize login screen
 */
error_code_t login_screen_init(void) {
    if (login_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing login screen...\n");
    
    memset(&login_state, 0, sizeof(login_screen_t));
    login_state.show_password = false;
    login_state.logged_in = false;
    login_state.username_widget = NULL;
    login_state.password_widget = NULL;
    login_state.login_button_widget = NULL;
    login_state.create_user_button_widget = NULL;
    login_state.initialized = true;
    
    kinfo("Login screen initialized\n");
    return ERR_OK;
}

/**
 * Show login screen
 */
error_code_t login_screen_show(void) {
    if (!login_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    login_state.logged_in = false;
    return ERR_OK;
}

/**
 * Hide login screen
 */
error_code_t login_screen_hide(void) {
    if (!login_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    // Clear credentials
    memset(login_state.username, 0, sizeof(login_state.username));
    memset(login_state.password, 0, sizeof(login_state.password));
    
    return ERR_OK;
}

/**
 * Render login screen with glassmorphism effect
 */
error_code_t login_screen_render(void) {
    if (!login_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return ERR_INVALID_STATE;
    }
    
    theme_t* theme = theme_get_current();
    if (!theme) {
        return ERR_INVALID_STATE;
    }
    
    // Draw background (gradient or wallpaper)
    extern error_code_t desktop_render(void);
    desktop_render();  // Use desktop wallpaper as background
    
    // Login card (centered, glassmorphism effect)
    uint32_t card_w = 400;
    uint32_t card_h = 300;
    uint32_t card_x = (fb->width - card_w) / 2;
    uint32_t card_y = (fb->height - card_h) / 2;
    
    // Glass card background
    gfx_draw_rect_alpha(card_x, card_y, card_w, card_h,
                       RGB(40, 40, 60), 200);  // Semi-transparent
    
    // Card border (glowing effect)
    gfx_draw_rect(card_x, card_y, card_w, card_h, RGB(100, 120, 180));
    gfx_draw_rect(card_x + 1, card_y + 1, card_w - 2, card_h - 2, RGB(60, 80, 120));
    
    // Title
    gfx_draw_string(card_x + (card_w / 2) - 40, card_y + 32, "Welcome",
                   RGB(255, 255, 255), 0);
    
    // Username field
    uint32_t field_x = card_x + 32;
    uint32_t field_y = card_y + 80;
    uint32_t field_w = card_w - 64;
    uint32_t field_h = 32;
    
    // Username label
    gfx_draw_string(field_x, field_y - 16, "Username:",
                   RGB(200, 200, 220), 0);
    
    // Username input (glass effect)
    gfx_draw_rect_alpha(field_x, field_y, field_w, field_h,
                       RGB(30, 30, 40), 240);
    gfx_draw_rect(field_x, field_y, field_w, field_h, RGB(80, 100, 120));
    
    // Username text
    if (login_state.username[0]) {
        gfx_draw_string(field_x + 8, field_y + 8, login_state.username,
                       RGB(255, 255, 255), 0);
    } else {
        gfx_draw_string(field_x + 8, field_y + 8, "Enter username...",
                       RGB(120, 120, 140), 0);
    }
    
    // Password field
    field_y += 60;
    
    // Password label
    gfx_draw_string(field_x, field_y - 16, "Password:",
                   RGB(200, 200, 220), 0);
    
    // Password input (glass effect)
    gfx_draw_rect_alpha(field_x, field_y, field_w, field_h,
                       RGB(30, 30, 40), 240);
    gfx_draw_rect(field_x, field_y, field_w, field_h, RGB(80, 100, 120));
    
    // Password text (masked)
    if (login_state.password[0]) {
        char masked[128];
        size_t len = strlen(login_state.password);
        for (size_t i = 0; i < len && i < sizeof(masked) - 1; i++) {
            masked[i] = '*';
        }
        masked[len < sizeof(masked) - 1 ? len : sizeof(masked) - 1] = '\0';
        gfx_draw_string(field_x + 8, field_y + 8, masked,
                       RGB(255, 255, 255), 0);
    } else {
        gfx_draw_string(field_x + 8, field_y + 8, "Enter password...",
                       RGB(120, 120, 140), 0);
    }
    
    // Login button
    uint32_t btn_x = card_x + 64;
    uint32_t btn_y = card_y + 200;
    uint32_t btn_w = 120;
    uint32_t btn_h = 36;
    
    gfx_draw_rect_alpha(btn_x, btn_y, btn_w, btn_h,
                       RGB(60, 100, 180), 220);
    gfx_draw_rect(btn_x, btn_y, btn_w, btn_h, RGB(100, 140, 220));
    gfx_draw_string(btn_x + 32, btn_y + 10, "Login",
                   RGB(255, 255, 255), 0);
    
    // Create User button
    btn_x += btn_w + 16;
    
    gfx_draw_rect_alpha(btn_x, btn_y, btn_w, btn_h,
                       RGB(50, 50, 70), 220);
    gfx_draw_rect(btn_x, btn_y, btn_w, btn_h, RGB(80, 80, 100));
    gfx_draw_string(btn_x + 16, btn_y + 10, "Create User",
                   RGB(255, 255, 255), 0);
    
    return ERR_OK;
}

/**
 * Handle login screen input
 */
error_code_t login_screen_handle_input(void) {
    if (!login_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    // Input handling would process keyboard/mouse events
    // For now, this is a placeholder
    
    return ERR_OK;
}

/**
 * Check if user is logged in
 */
bool login_screen_is_logged_in(void) {
    return login_state.initialized && login_state.logged_in;
}

/**
 * Get login screen instance
 */
login_screen_t* login_screen_get(void) {
    return login_state.initialized ? &login_state : NULL;
}

