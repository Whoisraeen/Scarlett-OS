/**
 * @file widget.c
 * @brief UI Widget system implementation
 */

#include "../include/ui/widget.h"
#include "../include/ui/theme.h"
#include "../include/drivers/ps2.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"

/**
 * Create a new widget
 */
widget_t* widget_create(widget_type_t type, widget_t* parent) {
    widget_t* widget = (widget_t*)kmalloc(sizeof(widget_t));
    if (!widget) {
        return NULL;
    }
    
    memset(widget, 0, sizeof(widget_t));
    
    widget->type = type;
    widget->flags = WIDGET_FLAG_VISIBLE | WIDGET_FLAG_ENABLED;
    widget->parent = parent;
    widget->bg_color = RGB(240, 240, 240);
    widget->fg_color = RGB(0, 0, 0);
    widget->children = NULL;
    widget->next = NULL;
    widget->prev = NULL;
    
    // Default margins and padding
    widget->margin_left = 0;
    widget->margin_right = 0;
    widget->margin_top = 0;
    widget->margin_bottom = 0;
    widget->padding_left = 4;
    widget->padding_right = 4;
    widget->padding_top = 4;
    widget->padding_bottom = 4;
    
    return widget;
}

/**
 * Destroy a widget and all its children
 */
error_code_t widget_destroy(widget_t* widget) {
    if (!widget) {
        return ERR_INVALID_ARG;
    }
    
    // Destroy all children
    widget_t* child = widget->children;
    while (child) {
        widget_t* next = child->next;
        widget_destroy(child);
        child = next;
    }
    
    // Remove from parent's child list
    if (widget->parent) {
        widget_remove_child(widget->parent, widget);
    }
    
    // Remove from sibling list
    if (widget->prev) {
        widget->prev->next = widget->next;
    }
    if (widget->next) {
        widget->next->prev = widget->prev;
    }
    
    // Free widget-specific data
    if (widget->data) {
        kfree(widget->data);
    }
    
    kfree(widget);
    return ERR_OK;
}

/**
 * Set widget position
 */
error_code_t widget_set_position(widget_t* widget, int32_t x, int32_t y) {
    if (!widget) {
        return ERR_INVALID_ARG;
    }
    
    widget->x = x;
    widget->y = y;
    return ERR_OK;
}

/**
 * Set widget size
 */
error_code_t widget_set_size(widget_t* widget, uint32_t width, uint32_t height) {
    if (!widget || width == 0 || height == 0) {
        return ERR_INVALID_ARG;
    }
    
    widget->width = width;
    widget->height = height;
    return ERR_OK;
}

/**
 * Set widget text
 */
error_code_t widget_set_text(widget_t* widget, const char* text) {
    if (!widget) {
        return ERR_INVALID_ARG;
    }
    
    if (text) {
        strncpy(widget->text, text, sizeof(widget->text) - 1);
        widget->text[sizeof(widget->text) - 1] = '\0';
    } else {
        widget->text[0] = '\0';
    }
    
    return ERR_OK;
}

/**
 * Set widget colors
 */
error_code_t widget_set_colors(widget_t* widget, uint32_t bg_color, uint32_t fg_color) {
    if (!widget) {
        return ERR_INVALID_ARG;
    }
    
    widget->bg_color = bg_color;
    widget->fg_color = fg_color;
    return ERR_OK;
}

/**
 * Set widget visibility
 */
error_code_t widget_set_visible(widget_t* widget, bool visible) {
    if (!widget) {
        return ERR_INVALID_ARG;
    }
    
    if (visible) {
        widget->flags |= WIDGET_FLAG_VISIBLE;
    } else {
        widget->flags &= ~WIDGET_FLAG_VISIBLE;
    }
    
    return ERR_OK;
}

/**
 * Set widget enabled state
 */
error_code_t widget_set_enabled(widget_t* widget, bool enabled) {
    if (!widget) {
        return ERR_INVALID_ARG;
    }
    
    if (enabled) {
        widget->flags |= WIDGET_FLAG_ENABLED;
    } else {
        widget->flags &= ~WIDGET_FLAG_ENABLED;
    }
    
    return ERR_OK;
}

/**
 * Add child widget
 */
error_code_t widget_add_child(widget_t* parent, widget_t* child) {
    if (!parent || !child) {
        return ERR_INVALID_ARG;
    }
    
    child->parent = parent;
    
    // Add to parent's child list
    child->next = parent->children;
    if (parent->children) {
        parent->children->prev = child;
    }
    parent->children = child;
    child->prev = NULL;
    
    return ERR_OK;
}

/**
 * Remove child widget
 */
error_code_t widget_remove_child(widget_t* parent, widget_t* child) {
    if (!parent || !child || child->parent != parent) {
        return ERR_INVALID_ARG;
    }
    
    if (child->prev) {
        child->prev->next = child->next;
    } else {
        parent->children = child->next;
    }
    
    if (child->next) {
        child->next->prev = child->prev;
    }
    
    child->parent = NULL;
    child->next = NULL;
    child->prev = NULL;
    
    return ERR_OK;
}

/**
 * Get absolute position of widget (recursive)
 */
static void widget_get_absolute_position(widget_t* widget, int32_t* abs_x, int32_t* abs_y) {
    *abs_x = widget->x;
    *abs_y = widget->y;
    
    widget_t* parent = widget->parent;
    while (parent) {
        *abs_x += parent->x + parent->padding_left;
        *abs_y += parent->y + parent->padding_top;
        parent = parent->parent;
    }
}

/**
 * Render a widget and its children
 */
error_code_t widget_render(widget_t* widget, window_t* window) {
    if (!widget || !window || !(widget->flags & WIDGET_FLAG_VISIBLE)) {
        return ERR_INVALID_ARG;
    }
    
    // Get absolute position
    int32_t abs_x, abs_y;
    widget_get_absolute_position(widget, &abs_x, &abs_y);
    
    // Calculate actual position including margins
    int32_t x = abs_x + widget->margin_left + window->x;
    int32_t y = abs_y + widget->margin_top + window->y;
    uint32_t width = widget->width - widget->margin_left - widget->margin_right;
    uint32_t height = widget->height - widget->margin_top - widget->margin_bottom;
    
    // Call custom draw callback if provided
    if (widget->on_draw) {
        widget->on_draw(widget);
    } else {
        // Default rendering based on widget type
        switch (widget->type) {
            case WIDGET_TYPE_BUTTON: {
                // Get theme colors
                theme_t* theme = theme_get_current();
                
                // Draw button background
                uint32_t bg = (widget->flags & WIDGET_FLAG_FOCUSED) ? 
                    theme->button_bg_pressed : theme->button_bg;
                gfx_fill_rounded_rect(x, y, width, height, 6, bg);
                
                // Draw button border
                uint32_t border = (widget->flags & WIDGET_FLAG_FOCUSED) ? 
                    theme->border_focused : theme->border;
                gfx_draw_rounded_rect(x, y, width, height, 6, border);
                
                // Draw button text (centered)
                if (widget->text[0]) {
                    uint32_t text_x = x + (width / 2) - (strlen(widget->text) * 4);
                    uint32_t text_y = y + (height / 2) - 4;
                    gfx_draw_string(text_x, text_y, widget->text, 
                        theme->button_fg, 0); // Transparent bg
                }
                break;
            }
            
            case WIDGET_TYPE_LABEL: {
                // Draw label text
                if (widget->text[0]) {
                    gfx_draw_string(x, y, widget->text, widget->fg_color, 0);
                }
                break;
            }
            
            case WIDGET_TYPE_TEXTBOX: {
                // Get theme colors
                theme_t* theme = theme_get_current();
                
                // Draw textbox background
                uint32_t bg = (widget->flags & WIDGET_FLAG_FOCUSED) ?
                    theme->textbox_bg_focused : theme->textbox_bg;
                gfx_fill_rounded_rect(x, y, width, height, 4, bg);
                
                // Draw textbox border
                uint32_t border_color = (widget->flags & WIDGET_FLAG_FOCUSED) ?
                    theme->border_focused : theme->border;
                gfx_draw_rounded_rect(x, y, width, height, 4, border_color);
                
                // Draw text
                if (widget->text[0]) {
                    gfx_draw_string(x + 4, y + (height/2) - 4, widget->text, 
                        theme->textbox_fg, 0);
                } else if (widget->data) {  // Placeholder text
                    char* placeholder = (char*)widget->data;
                    gfx_draw_string(x + 4, y + (height/2) - 4, placeholder, 
                        theme->textbox_placeholder, 0);
                }
                
                // Draw cursor if focused
                if (widget->flags & WIDGET_FLAG_FOCUSED) {
                    uint32_t cursor_x = x + 4 + (strlen(widget->text) * 8);
                    gfx_draw_line(cursor_x, y + 4, cursor_x, y + height - 4, 
                        theme->textbox_fg);
                }
                break;
            }
            
            case WIDGET_TYPE_CHECKBOX: {
                // Draw checkbox box
                gfx_fill_rounded_rect(x, y, 16, 16, 3, RGB(255, 255, 255));
                gfx_draw_rounded_rect(x, y, 16, 16, 3, RGB(100, 100, 100));
                
                // Draw checkmark if checked
                if (widget->data && *(bool*)widget->data) {
                    gfx_draw_line(x + 3, y + 8, x + 7, y + 12, RGB(0, 120, 215));
                    gfx_draw_line(x + 7, y + 12, x + 13, y + 4, RGB(0, 120, 215));
                }
                
                // Draw label text
                if (widget->text[0]) {
                    gfx_draw_string(x + 20, y + 4, widget->text, 
                        widget->fg_color, 0);
                }
                break;
            }
            
            case WIDGET_TYPE_PANEL: {
                // Get theme colors
                theme_t* theme = theme_get_current();
                
                // Draw panel background
                gfx_fill_rounded_rect(x, y, width, height, 8, theme->panel_bg);
                
                // Draw panel border
                gfx_draw_rounded_rect(x, y, width, height, 8, theme->panel_border);
                break;
            }
            
            case WIDGET_TYPE_MENU: {
                // Menu is a container, draw background if open
                bool* is_open = (bool*)widget->data;
                if (is_open && *is_open) {
                    // Get theme colors
                    theme_t* theme = theme_get_current();
                    gfx_fill_rounded_rect(x, y, width, height, 4, theme->menu_bg);
                    gfx_draw_rounded_rect(x, y, width, height, 4, theme->menu_border);
                }
                break;
            }
            
            case WIDGET_TYPE_MENU_ITEM: {
                // Get theme colors
                theme_t* theme = theme_get_current();
                
                // Draw menu item background
                bool* is_hovered = (bool*)widget->data;
                uint32_t bg = (is_hovered && *is_hovered) ? 
                    theme->menu_item_bg_hover : theme->menu_item_bg;
                gfx_fill_rect(x, y, width, height, bg);
                
                // Draw menu item text
                if (widget->text[0]) {
                    gfx_draw_string(x + 8, y + (height/2) - 4, widget->text, 
                        theme->menu_item_fg, 0);
                }
                
                // Draw separator if text starts with "-"
                if (widget->text[0] == '-' && widget->text[1] == '\0') {
                    gfx_draw_line(x + 4, y + height/2, x + width - 4, y + height/2, 
                        theme->menu_separator);
                }
                break;
            }
            
            default:
                // Default: just draw background
                gfx_fill_rect(x, y, width, height, widget->bg_color);
                break;
        }
    }
    
    // Render children
    widget_t* child = widget->children;
    while (child) {
        widget_render(child, window);
        child = child->next;
    }
    
    return ERR_OK;
}

/**
 * Handle mouse input for widget
 */
error_code_t widget_handle_mouse(widget_t* widget, int32_t mx, int32_t my, bool clicked) {
    if (!widget || !(widget->flags & WIDGET_FLAG_VISIBLE) || 
        !(widget->flags & WIDGET_FLAG_ENABLED)) {
        return ERR_INVALID_ARG;
    }
    
    // Check if mouse is within widget bounds (recursive)
    widget_t* current = widget;
    int32_t check_x = mx;
    int32_t check_y = my;
    
    while (current) {
        int32_t abs_x, abs_y;
        widget_get_absolute_position(current, &abs_x, &abs_y);
        
        int32_t x = abs_x + current->margin_left;
        int32_t y = abs_y + current->margin_top;
        uint32_t width = current->width - current->margin_left - current->margin_right;
        uint32_t height = current->height - current->margin_top - current->margin_bottom;
        
        if (check_x < x || check_x >= (int32_t)(x + width) ||
            check_y < y || check_y >= (int32_t)(y + height)) {
            return ERR_NOT_FOUND;  // Not in this widget's bounds
        }
        
        check_x -= (x - abs_x);
        check_y -= (y - abs_y);
        current = current->parent;
    }
    
    // Handle click
    if (clicked) {
        widget->flags |= WIDGET_FLAG_FOCUSED;
        
        // Widget-specific click handling
        switch (widget->type) {
            case WIDGET_TYPE_MENU:
                // Toggle menu open/closed
                {
                    bool is_open = widget_menu_is_open(widget);
                    widget_menu_set_open(widget, !is_open);
                }
                break;
                
            case WIDGET_TYPE_MENU_ITEM:
                // Menu item clicked - close parent menu and trigger callback
                {
                    widget_t* menu = widget->parent;
                    if (menu && menu->type == WIDGET_TYPE_MENU) {
                        widget_menu_set_open(menu, false);
                    }
                }
                // Fall through to call on_click
                break;
        }
        
        if (widget->on_click) {
            widget->on_click(widget, widget->user_data);
        }
        return ERR_OK;
    } else {
        // Handle hover for menu items
        if (widget->type == WIDGET_TYPE_MENU_ITEM && widget->data) {
            bool* is_hovered = (bool*)widget->data;
            *is_hovered = true;
        }
    }
    
    // Check children
    widget_t* child = widget->children;
    while (child) {
        if (widget_handle_mouse(child, mx, my, clicked) == ERR_OK) {
            return ERR_OK;
        }
        child = child->next;
    }
    
    return ERR_NOT_FOUND;
}

/**
 * Handle keyboard input for widget
 */
error_code_t widget_handle_keyboard(widget_t* widget, uint8_t key, char ascii) {
    if (!widget || !(widget->flags & WIDGET_FLAG_FOCUSED)) {
        return ERR_INVALID_ARG;
    }
    
    switch (widget->type) {
        case WIDGET_TYPE_TEXTBOX: {
            if (ascii >= 32 && ascii <= 126) {  // Printable character
                size_t len = strlen(widget->text);
                if (len < sizeof(widget->text) - 1) {
                    widget->text[len] = ascii;
                    widget->text[len + 1] = '\0';
                }
            } else if (key == KEY_BACKSPACE) {
                size_t len = strlen(widget->text);
                if (len > 0) {
                    widget->text[len - 1] = '\0';
                }
            }
            
            if (widget->on_change) {
                widget->on_change(widget, widget->user_data);
            }
            return ERR_OK;
        }
        
        case WIDGET_TYPE_BUTTON: {
            if (key == KEY_ENTER || ascii == ' ') {
                if (widget->on_click) {
                    widget->on_click(widget, widget->user_data);
                }
                return ERR_OK;
            }
            break;
        }
        
        case WIDGET_TYPE_CHECKBOX: {
            if (key == KEY_ENTER || ascii == ' ') {
                bool* checked = (bool*)widget->data;
                if (!checked) {
                    checked = (bool*)kmalloc(sizeof(bool));
                    *checked = false;
                    widget->data = checked;
                }
                *checked = !*checked;
                
                if (widget->on_change) {
                    widget->on_change(widget, widget->user_data);
                }
                return ERR_OK;
            }
            break;
        }
        
        default:
            break;
    }
    
    return ERR_NOT_SUPPORTED;
}

/**
 * Create a button widget
 */
widget_t* widget_create_button(widget_t* parent, const char* text, int32_t x, int32_t y, 
                                uint32_t width, uint32_t height) {
    widget_t* widget = widget_create(WIDGET_TYPE_BUTTON, parent);
    if (!widget) {
        return NULL;
    }
    
    widget_set_position(widget, x, y);
    widget_set_size(widget, width, height);
    widget_set_text(widget, text);
    widget_set_colors(widget, RGB(240, 240, 240), RGB(0, 0, 0));
    
    if (parent) {
        widget_add_child(parent, widget);
    }
    
    return widget;
}

/**
 * Create a label widget
 */
widget_t* widget_create_label(widget_t* parent, const char* text, int32_t x, int32_t y) {
    widget_t* widget = widget_create(WIDGET_TYPE_LABEL, parent);
    if (!widget) {
        return NULL;
    }
    
    widget_set_position(widget, x, y);
    widget_set_size(widget, strlen(text) * 8, 8);  // 8x8 font
    widget_set_text(widget, text);
    
    if (parent) {
        widget_add_child(parent, widget);
    }
    
    return widget;
}

/**
 * Create a textbox widget
 */
widget_t* widget_create_textbox(widget_t* parent, const char* placeholder, int32_t x, int32_t y,
                                 uint32_t width, uint32_t height) {
    widget_t* widget = widget_create(WIDGET_TYPE_TEXTBOX, parent);
    if (!widget) {
        return NULL;
    }
    
    widget_set_position(widget, x, y);
    widget_set_size(widget, width, height);
    widget_set_colors(widget, RGB(255, 255, 255), RGB(0, 0, 0));
    
    if (placeholder) {
        widget->data = (void*)strdup(placeholder);
    }
    
    if (parent) {
        widget_add_child(parent, widget);
    }
    
    return widget;
}

/**
 * Create a checkbox widget
 */
widget_t* widget_create_checkbox(widget_t* parent, const char* text, int32_t x, int32_t y) {
    widget_t* widget = widget_create(WIDGET_TYPE_CHECKBOX, parent);
    if (!widget) {
        return NULL;
    }
    
    widget_set_position(widget, x, y);
    widget_set_size(widget, 16 + strlen(text) * 8 + 4, 16);
    widget_set_text(widget, text);
    
    bool* checked = (bool*)kmalloc(sizeof(bool));
    *checked = false;
    widget->data = checked;
    
    if (parent) {
        widget_add_child(parent, widget);
    }
    
    return widget;
}

/**
 * Create a panel widget
 */
widget_t* widget_create_panel(widget_t* parent, int32_t x, int32_t y, uint32_t width, uint32_t height) {
    widget_t* widget = widget_create(WIDGET_TYPE_PANEL, parent);
    if (!widget) {
        return NULL;
    }
    
    widget_set_position(widget, x, y);
    widget_set_size(widget, width, height);
    widget_set_colors(widget, RGB(250, 250, 250), RGB(0, 0, 0));
    
    if (parent) {
        widget_add_child(parent, widget);
    }
    
    return widget;
}

