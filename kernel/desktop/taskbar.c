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

    // Modern glassmorphism taskbar design (Windows 11 style)
    // Full width bar, but content centered
    
    // Draw subtle shadow above taskbar for depth
    gfx_draw_shadow(taskbar_state.x, taskbar_state.y - 2,
                   taskbar_state.width, taskbar_state.height, 0, 15);

    // Draw frosted glass taskbar background
    // Windows 11 uses a very subtle, light/dark acrylic
    gfx_fill_rounded_rect_alpha(taskbar_state.x, taskbar_state.y,
                                taskbar_state.width, taskbar_state.height,
                                0, RGB(32, 32, 32), 200);  // Darker, more neutral

    // Draw subtle top border (glass reflection effect)
    uint32_t border_y = taskbar_state.y;
    gfx_draw_line(taskbar_state.x, border_y, taskbar_state.x + taskbar_state.width - 1, border_y,
                 RGBA(255, 255, 255, 40));

    // Calculate total width of content to center it
    uint32_t start_btn_w = 40;
    uint32_t item_spacing = 8;
    uint32_t content_width = start_btn_w;
    
    spinlock_lock(&taskbar_lock);
    taskbar_item_t* item = taskbar_state.items;
    while (item) {
        uint32_t item_width = 40; // Icons only for Windows 11 style, or fixed width with text?
        // Let's stick to icon-like buttons for now, or small pills
        // Windows 11 has icon-only taskbar buttons by default
        // But our system might not have icons yet, so let's use text but compact
        uint32_t text_width = strlen(item->title) * 8;
        item_width = text_width + 24; // Padding
        
        content_width += item_spacing + item_width;
        item = item->next;
    }
    spinlock_unlock(&taskbar_lock);

    // Calculate starting X for centered layout
    uint32_t start_x = (taskbar_state.width - content_width) / 2;
    
    // Draw Start Button (Centered)
    uint32_t start_btn_x = start_x;
    uint32_t start_btn_y = taskbar_state.y + 6;
    uint32_t start_btn_h = 36;
    uint32_t btn_radius = 8;

    // Start button background (Windows 11 logo blue-ish)
    gfx_fill_rounded_rect_alpha(start_btn_x, start_btn_y, start_btn_w, start_btn_h,
                                btn_radius, RGB(0, 120, 215), 220);
    
    // Start button icon (Four squares)
    uint32_t icon_x = start_btn_x + 12;
    uint32_t icon_y = start_btn_y + 10;
    uint32_t sq_size = 7;
    uint32_t gap = 2;
    uint32_t logo_col = RGB(255, 255, 255);
    
    gfx_fill_rect(icon_x, icon_y, sq_size, sq_size, logo_col);
    gfx_fill_rect(icon_x + sq_size + gap, icon_y, sq_size, sq_size, logo_col);
    gfx_fill_rect(icon_x, icon_y + sq_size + gap, sq_size, sq_size, logo_col);
    gfx_fill_rect(icon_x + sq_size + gap, icon_y + sq_size + gap, sq_size, sq_size, logo_col);

    // Render window items
    spinlock_lock(&taskbar_lock);
    uint32_t current_x = start_btn_x + start_btn_w + item_spacing;
    item = taskbar_state.items;

    while (item) {
        uint32_t text_width = strlen(item->title) * 8;
        uint32_t item_width = text_width + 24;
        uint32_t item_y = taskbar_state.y + 6;
        uint32_t item_h = 36;
        uint32_t item_radius = 6;

        // Item background
        if (item->active) {
            // Active: lighter background, bottom indicator
            gfx_fill_rounded_rect_alpha(current_x, item_y, item_width, item_h,
                                       item_radius, RGB(255, 255, 255), 30);
            
            // Bottom indicator line
            gfx_fill_rect(current_x + item_width/2 - 8, taskbar_state.y + taskbar_state.height - 3, 
                         16, 2, RGB(0, 120, 215));
        } else {
            // Inactive: very subtle hover effect (simulated) or transparent
            gfx_fill_rounded_rect_alpha(current_x, item_y, item_width, item_h,
                                       item_radius, RGB(255, 255, 255), 10);
        }

        // Item text
        gfx_draw_string(current_x + 12, item_y + 14, item->title,
                       RGB(255, 255, 255), 0);

        current_x += item_width + item_spacing;
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

