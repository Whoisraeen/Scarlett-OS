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
    
    spinlock_acquire(&taskbar_lock);
    
    // Check if window already in taskbar
    taskbar_item_t* item = taskbar_state.items;
    while (item) {
        if (item->window == window) {
            spinlock_release(&taskbar_lock);
            return ERR_ALREADY_EXISTS;
        }
        item = item->next;
    }
    
    // Create new taskbar item
    taskbar_item_t* new_item = (taskbar_item_t*)kmalloc(sizeof(taskbar_item_t));
    if (!new_item) {
        spinlock_release(&taskbar_lock);
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
    
    spinlock_release(&taskbar_lock);
    
    return ERR_OK;
}

/**
 * Remove window from taskbar
 */
error_code_t taskbar_remove_window(window_t* window) {
    if (!window || !taskbar_state.initialized) {
        return ERR_INVALID_ARG;
    }
    
    spinlock_acquire(&taskbar_lock);
    
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
            spinlock_release(&taskbar_lock);
            return ERR_OK;
        }
        prev = item;
        item = item->next;
    }
    
    spinlock_release(&taskbar_lock);
    return ERR_NOT_FOUND;
}

/**
 * Set active window in taskbar
 */
error_code_t taskbar_set_active_window(window_t* window) {
    if (!taskbar_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    spinlock_acquire(&taskbar_lock);
    
    taskbar_item_t* item = taskbar_state.items;
    while (item) {
        item->active = (item->window == window);
        item = item->next;
    }
    
    spinlock_release(&taskbar_lock);
    
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
    
    // Glassmorphism effect: semi-transparent background with blur
    // Draw taskbar with alpha blending (translucent)
    uint32_t taskbar_color = RGB(30, 30, 40);  // Dark blue-gray
    gfx_draw_rect_alpha(taskbar_state.x, taskbar_state.y, 
                       taskbar_state.width, taskbar_state.height,
                       taskbar_color, 180);  // 70% opacity
    
    // Draw border (subtle, glass-like)
    gfx_draw_rect(taskbar_state.x, taskbar_state.y, 
                 taskbar_state.width, 1, RGB(100, 100, 120));
    
    // Draw start button (glassmorphism style)
    uint32_t start_btn_x = 8;
    uint32_t start_btn_y = taskbar_state.y + 8;
    uint32_t start_btn_w = 32;
    uint32_t start_btn_h = 32;
    
    // Start button background (glass effect)
    gfx_draw_rect_alpha(start_btn_x, start_btn_y, start_btn_w, start_btn_h,
                       RGB(60, 60, 80), 200);
    gfx_draw_rect(start_btn_x, start_btn_y, start_btn_w, start_btn_h,
                 RGB(120, 120, 140));
    
    // Start button icon (hamburger menu or logo)
    gfx_draw_string(start_btn_x + 8, start_btn_y + 8, "☰", 
                   RGB(255, 255, 255), 0);
    
    // Render window items
    spinlock_acquire(&taskbar_lock);
    
    uint32_t item_x = start_btn_x + start_btn_w + 8;
    taskbar_item_t* item = taskbar_state.items;
    
    while (item) {
        uint32_t item_width = strlen(item->title) * 8 + 16;  // Approximate width
        uint32_t item_y = taskbar_state.y + 8;
        uint32_t item_h = 32;
        
        // Item background (different if active)
        uint32_t item_bg = item->active ? RGB(80, 80, 100) : RGB(50, 50, 70);
        gfx_draw_rect_alpha(item_x, item_y, item_width, item_h, item_bg, 200);
        
        // Item border
        uint32_t border_color = item->active ? RGB(100, 150, 255) : RGB(80, 80, 100);
        gfx_draw_rect(item_x, item_y, item_width, item_h, border_color);
        
        // Item text
        gfx_draw_string(item_x + 8, item_y + 10, item->title,
                       RGB(255, 255, 255), 0);
        
        item_x += item_width + 4;
        item = item->next;
    }
    
    spinlock_release(&taskbar_lock);
    
    return ERR_OK;
}

/**
 * Get taskbar instance
 */
taskbar_t* taskbar_get(void) {
    return taskbar_state.initialized ? &taskbar_state : NULL;
}

