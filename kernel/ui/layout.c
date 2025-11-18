/**
 * @file layout.c
 * @brief UI Layout management implementation
 */

#include "../include/ui/layout.h"
#include "../include/ui/widget.h"
#include "../include/kprintf.h"
#include "../include/mm/heap.h"

// Layout data structure
typedef struct {
    layout_type_t type;
    uint32_t spacing;  // Spacing between children
    uint32_t columns;  // For grid layout
} layout_data_t;

/**
 * Set layout type for widget
 */
error_code_t layout_set_type(widget_t* widget, layout_type_t type) {
    if (!widget) {
        return ERR_INVALID_ARG;
    }
    
    // Allocate layout data if needed
    layout_data_t* layout = (layout_data_t*)widget->data;
    if (!layout) {
        layout = (layout_data_t*)kmalloc(sizeof(layout_data_t));
        if (!layout) {
            return ERR_OUT_OF_MEMORY;
        }
        widget->data = layout;
    }
    
    layout->type = type;
    layout->spacing = 4;  // Default spacing
    layout->columns = 1;
    
    return ERR_OK;
}

/**
 * Update widget layout
 */
error_code_t layout_update(widget_t* widget) {
    if (!widget || !widget->data) {
        return ERR_INVALID_ARG;
    }
    
    layout_data_t* layout = (layout_data_t*)widget->data;
    
    if (layout->type == LAYOUT_TYPE_NONE) {
        return ERR_OK;  // No layout to update
    }
    
    // Get content area (excluding padding)
    uint32_t content_width = widget->width - widget->padding_left - widget->padding_right;
    uint32_t content_height = widget->height - widget->padding_top - widget->padding_bottom;
    
    int32_t x = widget->padding_left;
    int32_t y = widget->padding_top;
    
    widget_t* child = widget->children;
    uint32_t child_count = 0;
    
    // Count children
    while (child) {
        child_count++;
        child = child->next;
    }
    
    if (child_count == 0) {
        return ERR_OK;
    }
    
    switch (layout->type) {
        case LAYOUT_TYPE_VERTICAL: {
            // Calculate child height
            uint32_t total_spacing = layout->spacing * (child_count - 1);
            uint32_t child_height = (content_height - total_spacing) / child_count;
            
            child = widget->children;
            while (child) {
                widget_set_position(child, x, y);
                widget_set_size(child, content_width, child_height);
                y += child_height + layout->spacing;
                child = child->next;
            }
            break;
        }
        
        case LAYOUT_TYPE_HORIZONTAL: {
            // Calculate child width
            uint32_t total_spacing = layout->spacing * (child_count - 1);
            uint32_t child_width = (content_width - total_spacing) / child_count;
            
            child = widget->children;
            while (child) {
                widget_set_position(child, x, y);
                widget_set_size(child, child_width, content_height);
                x += child_width + layout->spacing;
                child = child->next;
            }
            break;
        }
        
        case LAYOUT_TYPE_GRID: {
            uint32_t cols = layout->columns;
            if (cols == 0) cols = 1;
            
            uint32_t rows = (child_count + cols - 1) / cols;
            uint32_t total_h_spacing = layout->spacing * (cols - 1);
            uint32_t total_v_spacing = layout->spacing * (rows - 1);
            uint32_t child_width = (content_width - total_h_spacing) / cols;
            uint32_t child_height = (content_height - total_v_spacing) / rows;
            
            child = widget->children;
            uint32_t index = 0;
            while (child) {
                uint32_t col = index % cols;
                uint32_t row = index / cols;
                
                int32_t child_x = x + col * (child_width + layout->spacing);
                int32_t child_y = y + row * (child_height + layout->spacing);
                
                widget_set_position(child, child_x, child_y);
                widget_set_size(child, child_width, child_height);
                
                child = child->next;
                index++;
            }
            break;
        }
        
        default:
            break;
    }
    
    return ERR_OK;
}

