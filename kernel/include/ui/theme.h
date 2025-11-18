/**
 * @file theme.h
 * @brief UI Theme system
 */

#ifndef KERNEL_UI_THEME_H
#define KERNEL_UI_THEME_H

#include "../types.h"
#include "../graphics/framebuffer.h"

// Theme structure
typedef struct {
    // Colors
    uint32_t bg_primary;      // Primary background
    uint32_t bg_secondary;    // Secondary background
    uint32_t bg_tertiary;     // Tertiary background
    uint32_t fg_primary;      // Primary foreground (text)
    uint32_t fg_secondary;    // Secondary foreground
    uint32_t accent;          // Accent color
    uint32_t border;          // Border color
    uint32_t border_focused; // Focused border color
    
    // Button colors
    uint32_t button_bg;
    uint32_t button_bg_hover;
    uint32_t button_bg_pressed;
    uint32_t button_fg;
    
    // Textbox colors
    uint32_t textbox_bg;
    uint32_t textbox_bg_focused;
    uint32_t textbox_fg;
    uint32_t textbox_placeholder;
    
    // Panel colors
    uint32_t panel_bg;
    uint32_t panel_border;
    
    // Font
    uint32_t font_size;      // Font size (currently 8x8)
} theme_t;

// Predefined themes
extern theme_t theme_light;
extern theme_t theme_dark;
extern theme_t theme_blue;

// Theme functions
error_code_t theme_init(void);
theme_t* theme_get_current(void);
error_code_t theme_set_current(theme_t* theme);
error_code_t theme_apply_to_widget(void* widget, theme_t* theme);

#endif // KERNEL_UI_THEME_H

