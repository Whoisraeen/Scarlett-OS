/**
 * @file layout.h
 * @brief UI Layout management
 */

#ifndef KERNEL_UI_LAYOUT_H
#define KERNEL_UI_LAYOUT_H

#include "../types.h"
#include "../errors.h"
#include "widget.h"

// Layout types
typedef enum {
    LAYOUT_TYPE_NONE,        // No layout (absolute positioning)
    LAYOUT_TYPE_VERTICAL,    // Vertical box layout
    LAYOUT_TYPE_HORIZONTAL,  // Horizontal box layout
    LAYOUT_TYPE_GRID         // Grid layout
} layout_type_t;

// Layout functions
error_code_t layout_set_type(widget_t* widget, layout_type_t type);
error_code_t layout_update(widget_t* widget);

#endif // KERNEL_UI_LAYOUT_H

