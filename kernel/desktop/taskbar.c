/**
 * @file taskbar.c
 * @brief Taskbar/Panel implementation
 */

#include "../include/desktop/taskbar.h"
#include "../include/ui/widget.h"
#include "../include/ui/layout.h"
#include "../include/ui/theme.h"
#include "../include/graphics/graphics.h"
#include "../include/graphics/framebuffer.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"
#include "../include/sync/spinlock.h"

// Taskbar state
static taskbar_t taskbar_state = {0};
static spinlock_t taskbar_lock;

/**
 * Initialize taskbar
 */
error_code_t taskbar_init(void) {
    if (taskbar_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing taskbar...\n");
    
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return ERR_INVALID_STATE;
    }
    
    // Set taskbar position and size
    taskbar_state.position = TASKBAR_POSITION_BOTTOM;
    taskbar_state.height_px = 48;  // 48 pixels height
    taskbar_state.width = fb->width;
    taskbar_state.height = taskbar_state.height_px;
    taskbar_state.x = 0;
    taskbar_state.y = fb->height - taskbar_state.height_px;
    taskbar_state.auto_hide = false;
    taskbar_state.items = NULL;
    taskbar_state.start_button_widget = NULL;
    
    spinlock_init(&taskbar_lock);
    taskbar_state.initialized = true;
    
    kinfo("Taskbar initialized (%ux%u at %u,%u)\n", 
          taskbar_state.width, taskbar_state.height,
          taskbar_state.x, taskbar_state.y);
    
    return ERR_OK;
}

/**
 * Add window to taskbar
 */
error_code_t taskbar_add_window(window_t* window) {
    if (!window || !taskbar_state.initialized) {
        return ERR_INVALID_ARG;
    }
    
    spinlock_lock(&taskbar_lock);
    
    // Check if window already in taskbar
    taskbar_item_t* item = taskbar_state.items;
    while (item) {
        if (item->window == window) {
            spinlock_unlock(&taskbar_lock);
            return ERR_ALREADY_EXISTS;
        }
        item = item->next;
    }
    
    // Create new taskbar item
    taskbar_item_t* new_item = (taskbar_item_t*)kmalloc(sizeof(taskbar_item_t));
    if (!new_item) {
        spinlock_unlock(&taskbar_lock);
        return ERR_OUT_OF_MEMORY;
    }
    
    memset(new_item, 0, sizeof(taskbar_item_t));
    new_item->window = window;
    strncpy(new_item->title, window->title, sizeof(new_item->title) - 1);
    new_item->title[sizeof(new_item->title) - 1] = '\0';
    new_item->active = false;
    
    // Add to list
    new_item->next = taskbar_state.items;
    taskbar_state.items = new_item;
    
    spinlock_unlock(&taskbar_lock);
    
    return ERR_OK;
}

/**
 * Remove window from taskbar
 */
error_code_t taskbar_remove_window(window_t* window) {
    if (!window || !taskbar_state.initialized) {
        return ERR_INVALID_ARG;
    }
    
    spinlock_lock(&taskbar_lock);
    
    taskbar_item_t* item = taskbar_state.items;
    taskbar_item_t* prev = NULL;
    
    while (item) {
        if (item->window == window) {
            if (prev) {
                prev->next = item->next;
            } else {
                taskbar_state.items = item->next;
            }
            kfree(item);
            spinlock_unlock(&taskbar_lock);
            return ERR_OK;
        }
        prev = item;
        item = item->next;
    }
    
    spinlock_unlock(&taskbar_lock);
    return ERR_NOT_FOUND;
}

/**
 * Set active window in taskbar
 */
error_code_t taskbar_set_active_window(window_t* window) {
    if (!taskbar_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    spinlock_lock(&taskbar_lock);
    
    taskbar_item_t* item = taskbar_state.items;
    while (item) {
        item->active = (item->window == window);
        item = item->next;
    }
    
    spinlock_unlock(&taskbar_lock);
    
    return ERR_OK;
}

/**
 * Render taskbar with glassmorphism effect
 */
error_code_t taskbar_render(void) {
    if (!taskbar_state.initialized) {
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

    // Modern glassmorphism taskbar design
    uint32_t corner_radius = 0;  // No corner radius for taskbar (spans full width)

    // Draw subtle shadow above taskbar for depth
    gfx_draw_shadow(taskbar_state.x, taskbar_state.y - 3,
                   taskbar_state.width, taskbar_state.height, 0, 20);

    // Draw frosted glass taskbar background
    gfx_fill_rounded_rect_alpha(taskbar_state.x, taskbar_state.y,
                                taskbar_state.width, taskbar_state.height,
                                corner_radius, RGB(30, 38, 55), 220);  // Semi-transparent

    // Draw subtle top border (glass reflection effect)
    uint32_t border_x = taskbar_state.x;
    uint32_t border_y = taskbar_state.y;
    gfx_draw_line(border_x, border_y, border_x + taskbar_state.width - 1, border_y,
                 RGBA(255, 255, 255, 60));

    // Draw start button with modern glassmorphism
    uint32_t start_btn_x = 12;
    uint32_t start_btn_y = taskbar_state.y + 8;
    uint32_t start_btn_w = 36;
    uint32_t start_btn_h = 32;
    uint32_t btn_radius = 12;  // Heavily rounded corners

    // Start button with glass effect and rounded corners
    gfx_fill_rounded_rect_alpha(start_btn_x, start_btn_y, start_btn_w, start_btn_h,
                                btn_radius, RGB(80, 95, 125), 200);

    // Start button border (subtle glow)
    gfx_draw_rounded_rect(start_btn_x, start_btn_y, start_btn_w, start_btn_h,
                         btn_radius, RGBA(255, 255, 255, 80));

    // Start button icon (modern grid icon)
    gfx_draw_string(start_btn_x + 10, start_btn_y + 12, ":::",
                   RGB(255, 255, 255), 0);

    // Render window items with glassmorphism
    spinlock_lock(&taskbar_lock);

    uint32_t item_x = start_btn_x + start_btn_w + 12;
    taskbar_item_t* item = taskbar_state.items;

    while (item) {
        uint32_t item_width = strlen(item->title) * 8 + 20;  // Padding
        uint32_t item_y = taskbar_state.y + 8;
        uint32_t item_h = 32;
        uint32_t item_radius = 10;  // Rounded corners for items

        // Item background with glass effect (brighter if active)
        if (item->active) {
            // Active window - brighter glass with accent color
            gfx_draw_shadow(item_x - 1, item_y - 1, item_width + 2, item_h + 2, item_radius, 15);
            gfx_fill_rounded_rect_alpha(item_x, item_y, item_width, item_h,
                                       item_radius, RGB(100, 120, 160), 230);
            gfx_draw_rounded_rect(item_x, item_y, item_width, item_h,
                                 item_radius, RGBA(120, 160, 255, 150));
        } else {
            // Inactive window - subtle glass
            gfx_fill_rounded_rect_alpha(item_x, item_y, item_width, item_h,
                                       item_radius, RGB(60, 70, 95), 180);
            gfx_draw_rounded_rect(item_x, item_y, item_width, item_h,
                                 item_radius, RGBA(255, 255, 255, 40));
        }

        // Item text with proper contrast
        gfx_draw_string(item_x + 10, item_y + 12, item->title,
                       RGB(255, 255, 255), 0);

        item_x += item_width + 8;
        item = item->next;
    }

    spinlock_unlock(&taskbar_lock);

    return ERR_OK;
}

/**
 * Get taskbar instance
 */
taskbar_t* taskbar_get(void) {
    return taskbar_state.initialized ? &taskbar_state : NULL;
}

