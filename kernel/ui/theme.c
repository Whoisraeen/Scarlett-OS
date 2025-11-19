/**
 * @file theme.c
 * @brief UI Theme system implementation
 */

#include "../include/ui/theme.h"
#include "../include/ui/widget.h"
#include "../include/kprintf.h"

// Light theme
theme_t theme_light = {
    .bg_primary = RGB(255, 255, 255),
    .bg_secondary = RGB(240, 240, 240),
    .bg_tertiary = RGB(220, 220, 220),
    .fg_primary = RGB(0, 0, 0),
    .fg_secondary = RGB(100, 100, 100),
    .accent = RGB(0, 120, 215),
    .border = RGB(200, 200, 200),
    .border_focused = RGB(0, 120, 215),
    .button_bg = RGB(240, 240, 240),
    .button_bg_hover = RGB(220, 220, 220),
    .button_bg_pressed = RGB(200, 200, 200),
    .button_fg = RGB(0, 0, 0),
    .textbox_bg = RGB(255, 255, 255),
    .textbox_bg_focused = RGB(255, 255, 255),
    .textbox_fg = RGB(0, 0, 0),
    .textbox_placeholder = RGB(150, 150, 150),
    .panel_bg = RGB(250, 250, 250),
    .panel_border = RGB(200, 200, 200),
    .font_size = 8
};

// Dark theme
theme_t theme_dark = {
    .bg_primary = RGB(30, 30, 30),
    .bg_secondary = RGB(40, 40, 40),
    .bg_tertiary = RGB(50, 50, 50),
    .fg_primary = RGB(255, 255, 255),
    .fg_secondary = RGB(200, 200, 200),
    .accent = RGB(0, 150, 255),
    .border = RGB(80, 80, 80),
    .border_focused = RGB(0, 150, 255),
    .button_bg = RGB(50, 50, 50),
    .button_bg_hover = RGB(70, 70, 70),
    .button_bg_pressed = RGB(90, 90, 90),
    .button_fg = RGB(255, 255, 255),
    .textbox_bg = RGB(40, 40, 40),
    .textbox_bg_focused = RGB(50, 50, 50),
    .textbox_fg = RGB(255, 255, 255),
    .textbox_placeholder = RGB(150, 150, 150),
    .panel_bg = RGB(35, 35, 35),
    .panel_border = RGB(80, 80, 80),
    .font_size = 8
};

// Blue theme
theme_t theme_blue = {
    .bg_primary = RGB(240, 245, 255),
    .bg_secondary = RGB(220, 230, 255),
    .bg_tertiary = RGB(200, 215, 255),
    .fg_primary = RGB(0, 0, 0),
    .fg_secondary = RGB(50, 50, 100),
    .accent = RGB(0, 100, 200),
    .border = RGB(150, 170, 220),
    .border_focused = RGB(0, 100, 200),
    .button_bg = RGB(200, 220, 255),
    .button_bg_hover = RGB(180, 200, 255),
    .button_bg_pressed = RGB(160, 180, 255),
    .button_fg = RGB(0, 0, 0),
    .textbox_bg = RGB(255, 255, 255),
    .textbox_bg_focused = RGB(240, 250, 255),
    .textbox_fg = RGB(0, 0, 0),
    .textbox_placeholder = RGB(150, 150, 200),
    .panel_bg = RGB(245, 250, 255),
    .panel_border = RGB(150, 170, 220),
    .font_size = 8
};

// Glassmorphism theme (inspired by modern iOS/macOS design)
theme_t theme_glass = {
    .bg_primary = RGB(25, 30, 45),           // Deep blue-gray background
    .bg_secondary = RGB(35, 42, 60),          // Slightly lighter blue-gray
    .bg_tertiary = RGB(45, 54, 75),           // Medium blue-gray
    .fg_primary = RGB(255, 255, 255),         // White text
    .fg_secondary = RGB(180, 190, 210),       // Light gray-blue text
    .accent = RGB(100, 150, 255),             // Bright blue accent
    .border = RGBA(255, 255, 255, 60),        // Semi-transparent white border
    .border_focused = RGBA(100, 150, 255, 200), // Bright blue focused border
    .button_bg = RGBA(80, 90, 120, 180),      // Translucent glass button
    .button_bg_hover = RGBA(100, 110, 140, 200),
    .button_bg_pressed = RGBA(120, 130, 160, 220),
    .button_fg = RGB(255, 255, 255),
    .textbox_bg = RGBA(40, 48, 70, 200),      // Frosted glass textbox
    .textbox_bg_focused = RGBA(50, 58, 80, 220),
    .textbox_fg = RGB(255, 255, 255),
    .textbox_placeholder = RGB(120, 130, 150),
    .panel_bg = RGBA(30, 38, 55, 220),        // Frosted glass panel
    .panel_border = RGBA(255, 255, 255, 40),
    .font_size = 8
};

// Current theme
static theme_t* current_theme = &theme_glass;

/**
 * Initialize theme system
 */
error_code_t theme_init(void) {
    current_theme = &theme_light;  // Default to light theme
    return ERR_OK;
}

/**
 * Get current theme
 */
theme_t* theme_get_current(void) {
    return current_theme;
}

/**
 * Set current theme
 */
error_code_t theme_set_current(theme_t* theme) {
    if (!theme) {
        return ERR_INVALID_ARG;
    }
    
    current_theme = theme;
    return ERR_OK;
}

/**
 * Apply theme to widget
 */
error_code_t theme_apply_to_widget(void* widget_ptr, theme_t* theme) {
    if (!widget_ptr || !theme) {
        return ERR_INVALID_ARG;
    }
    
    widget_t* widget = (widget_t*)widget_ptr;
    
    switch (widget->type) {
        case WIDGET_TYPE_BUTTON:
            widget->bg_color = theme->button_bg;
            widget->fg_color = theme->button_fg;
            break;
            
        case WIDGET_TYPE_LABEL:
            widget->fg_color = theme->fg_primary;
            break;
            
        case WIDGET_TYPE_TEXTBOX:
            widget->bg_color = theme->textbox_bg;
            widget->fg_color = theme->textbox_fg;
            break;
            
        case WIDGET_TYPE_PANEL:
            widget->bg_color = theme->panel_bg;
            break;
            
        default:
            widget->bg_color = theme->bg_secondary;
            widget->fg_color = theme->fg_primary;
            break;
    }
    
    // Apply to children
    widget_t* child = widget->children;
    while (child) {
        theme_apply_to_widget(child, theme);
        child = child->next;
    }
    
    return ERR_OK;
}

