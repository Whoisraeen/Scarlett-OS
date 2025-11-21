/**
 * @file widget_menu.c
 * @brief Menu widget implementation
 */

#include "../include/ui/widget.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"

/**
 * Create a menu widget
 */
widget_t* widget_create_menu(widget_t* parent, int32_t x, int32_t y, uint32_t width) {
    widget_t* widget = widget_create(WIDGET_TYPE_MENU, parent);
    if (!widget) {
        return NULL;
    }
    
    widget_set_position(widget, x, y);
    widget_set_size(widget, width, 0);  // Height will be calculated from items
    widget_set_colors(widget, RGB(255, 255, 255), RGB(0, 0, 0));
    
    // Menu-specific data: is_open flag
    bool* is_open = (bool*)kmalloc(sizeof(bool));
    *is_open = false;
    widget->data = is_open;
    
    if (parent) {
        widget_add_child(parent, widget);
    }
    
    return widget;
}

/**
 * Create a menu item widget
 */
widget_t* widget_create_menu_item(widget_t* menu, const char* text) {
    if (!menu || menu->type != WIDGET_TYPE_MENU) {
        return NULL;
    }
    
    widget_t* widget = widget_create(WIDGET_TYPE_MENU_ITEM, menu);
    if (!widget) {
        return NULL;
    }
    
    // Calculate position based on existing items
    uint32_t item_height = 24;
    uint32_t item_count = 0;
    widget_t* child = menu->children;
    while (child) {
        if (child->type == WIDGET_TYPE_MENU_ITEM) {
            item_count++;
        }
        child = child->next;
    }
    
    widget_set_position(widget, 0, item_count * item_height);
    widget_set_size(widget, menu->width, item_height);
    widget_set_text(widget, text);
    widget_set_colors(widget, RGB(255, 255, 255), RGB(0, 0, 0));
    
    // Menu item-specific data: is_hovered flag
    bool* is_hovered = (bool*)kmalloc(sizeof(bool));
    *is_hovered = false;
    widget->data = is_hovered;
    
    // Update menu height
    menu->height = (item_count + 1) * item_height;
    
    widget_add_child(menu, widget);
    
    return widget;
}

/**
 * Add item to menu
 */
error_code_t widget_menu_add_item(widget_t* menu, widget_t* item) {
    if (!menu || menu->type != WIDGET_TYPE_MENU || !item) {
        return ERR_INVALID_ARG;
    }
    
    if (item->type != WIDGET_TYPE_MENU_ITEM) {
        return ERR_INVALID_ARG;
    }
    
    // Calculate position
    uint32_t item_height = 24;
    uint32_t item_count = 0;
    widget_t* child = menu->children;
    while (child) {
        if (child->type == WIDGET_TYPE_MENU_ITEM) {
            item_count++;
        }
        child = child->next;
    }
    
    widget_set_position(item, 0, item_count * item_height);
    widget_set_size(item, menu->width, item_height);
    
    // Update menu height
    menu->height = (item_count + 1) * item_height;
    
    return widget_add_child(menu, item);
}

/**
 * Set menu open state
 */
error_code_t widget_menu_set_open(widget_t* menu, bool open) {
    if (!menu || menu->type != WIDGET_TYPE_MENU) {
        return ERR_INVALID_ARG;
    }
    
    bool* is_open = (bool*)menu->data;
    if (is_open) {
        *is_open = open;
    }
    
    // Update visibility of menu items
    widget_t* child = menu->children;
    while (child) {
        if (open) {
            child->flags |= WIDGET_FLAG_VISIBLE;
        } else {
            child->flags &= ~WIDGET_FLAG_VISIBLE;
        }
        child = child->next;
    }
    
    return ERR_OK;
}

/**
 * Check if menu is open
 */
bool widget_menu_is_open(widget_t* menu) {
    if (!menu || menu->type != WIDGET_TYPE_MENU) {
        return false;
    }
    
    bool* is_open = (bool*)menu->data;
    return (is_open && *is_open);
}

