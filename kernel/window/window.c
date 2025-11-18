/**
 * @file window.c
 * @brief Window manager implementation
 */

#include "../include/window/window.h"
#include "../include/input/input.h"
#include "../include/graphics/framebuffer.h"
#include "../include/graphics/graphics.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"
#include "../include/sync/spinlock.h"

// Window manager state
static struct {
    window_t* windows;
    window_t* focused_window;
    uint64_t next_window_id;
    spinlock_t lock;
    bool initialized;
} wm_state = {0};

/**
 * Initialize window manager
 */
error_code_t window_manager_init(void) {
    if (wm_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing window manager...\n");
    
    wm_state.windows = NULL;
    wm_state.focused_window = NULL;
    wm_state.next_window_id = 1;
    spinlock_init(&wm_state.lock);
    wm_state.initialized = true;
    
    // Initialize double buffering for graphics
    gfx_init_double_buffer();
    
    kinfo("Window manager initialized\n");
    return ERR_OK;
}

/**
 * Create a new window
 */
window_t* window_create(int32_t x, int32_t y, uint32_t width, uint32_t height, const char* title) {
    if (!wm_state.initialized) {
        return NULL;
    }
    
    // Allocate window structure
    window_t* window = (window_t*)kmalloc(sizeof(window_t));
    if (!window) {
        return NULL;
    }
    
    // Allocate window buffer
    size_t buffer_size = width * height * sizeof(uint32_t);
    window->buffer = (uint32_t*)kmalloc(buffer_size);
    if (!window->buffer) {
        kfree(window);
        return NULL;
    }
    
    // Initialize window
    window->id = wm_state.next_window_id++;
    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->flags = WINDOW_FLAG_VISIBLE | WINDOW_FLAG_FOCUSED;
    window->bg_color = RGB(240, 240, 240);  // Light gray
    window->widget_root = NULL;  // No widgets initially
    window->next = NULL;
    window->prev = NULL;
    
    if (title) {
        strncpy(window->title, title, sizeof(window->title) - 1);
        window->title[sizeof(window->title) - 1] = '\0';
    } else {
        window->title[0] = '\0';
    }
    
    // Clear window buffer
    memset(window->buffer, 0xFF, buffer_size);
    
    // Add to window list
    spinlock_acquire(&wm_state.lock);
    window->next = wm_state.windows;
    if (wm_state.windows) {
        wm_state.windows->prev = window;
    }
    wm_state.windows = window;
    
    // Set as focused if no other window is focused
    if (!wm_state.focused_window) {
        wm_state.focused_window = window;
    } else {
        // Unfocus previous window
        wm_state.focused_window->flags &= ~WINDOW_FLAG_FOCUSED;
        wm_state.focused_window = window;
    }
    spinlock_release(&wm_state.lock);
    
    kinfo("Created window %lu: %s (%dx%d at %d,%d)\n", 
          window->id, window->title, width, height, x, y);
    
    return window;
}

/**
 * Destroy a window
 */
error_code_t window_destroy(window_t* window) {
    if (!window || !wm_state.initialized) {
        return ERR_INVALID_ARG;
    }
    
    spinlock_acquire(&wm_state.lock);
    
    // Remove from list
    if (window->prev) {
        window->prev->next = window->next;
    } else {
        wm_state.windows = window->next;
    }
    
    if (window->next) {
        window->next->prev = window->prev;
    }
    
    // Update focused window
    if (wm_state.focused_window == window) {
        wm_state.focused_window = wm_state.windows;  // Focus first window
        if (wm_state.focused_window) {
            wm_state.focused_window->flags |= WINDOW_FLAG_FOCUSED;
        }
    }
    
    spinlock_release(&wm_state.lock);
    
    // Free resources
    if (window->buffer) {
        kfree(window->buffer);
    }
    kfree(window);
    
    return ERR_OK;
}

/**
 * Set window title
 */
error_code_t window_set_title(window_t* window, const char* title) {
    if (!window) {
        return ERR_INVALID_ARG;
    }
    
    if (title) {
        strncpy(window->title, title, sizeof(window->title) - 1);
        window->title[sizeof(window->title) - 1] = '\0';
    } else {
        window->title[0] = '\0';
    }
    
    return ERR_OK;
}

/**
 * Set window position
 */
error_code_t window_set_position(window_t* window, int32_t x, int32_t y) {
    if (!window) {
        return ERR_INVALID_ARG;
    }
    
    window->x = x;
    window->y = y;
    
    return ERR_OK;
}

/**
 * Set window size
 */
error_code_t window_set_size(window_t* window, uint32_t width, uint32_t height) {
    if (!window || width == 0 || height == 0) {
        return ERR_INVALID_ARG;
    }
    
    // Reallocate buffer if size changed
    if (width != window->width || height != window->height) {
        if (window->buffer) {
            kfree(window->buffer);
        }
        
        size_t buffer_size = width * height * sizeof(uint32_t);
        window->buffer = (uint32_t*)kmalloc(buffer_size);
        if (!window->buffer) {
            return ERR_OUT_OF_MEMORY;
        }
        
        // Clear new buffer
        memset(window->buffer, 0xFF, buffer_size);
    }
    
    window->width = width;
    window->height = height;
    
    return ERR_OK;
}

/**
 * Set window visibility
 */
error_code_t window_set_visible(window_t* window, bool visible) {
    if (!window) {
        return ERR_INVALID_ARG;
    }
    
    if (visible) {
        window->flags |= WINDOW_FLAG_VISIBLE;
    } else {
        window->flags &= ~WINDOW_FLAG_VISIBLE;
    }
    
    return ERR_OK;
}

/**
 * Set window focus
 */
error_code_t window_set_focus(window_t* window, bool focused) {
    if (!window) {
        return ERR_INVALID_ARG;
    }
    
    spinlock_acquire(&wm_state.lock);
    
    if (focused) {
        // Unfocus previous window
        if (wm_state.focused_window && wm_state.focused_window != window) {
            wm_state.focused_window->flags &= ~WINDOW_FLAG_FOCUSED;
        }
        
        window->flags |= WINDOW_FLAG_FOCUSED;
        wm_state.focused_window = window;
    } else {
        window->flags &= ~WINDOW_FLAG_FOCUSED;
        if (wm_state.focused_window == window) {
            wm_state.focused_window = NULL;
        }
    }
    
    spinlock_release(&wm_state.lock);
    
    return ERR_OK;
}

/**
 * Get focused window
 */
window_t* window_get_focused(void) {
    return wm_state.focused_window;
}

/**
 * Find window at coordinates
 */
window_t* window_find_at(int32_t x, int32_t y) {
    spinlock_acquire(&wm_state.lock);
    
    // Search from front to back (top to bottom in Z-order)
    window_t* window = wm_state.windows;
    while (window) {
        if ((window->flags & WINDOW_FLAG_VISIBLE) &&
            x >= window->x && x < (int32_t)(window->x + window->width) &&
            y >= window->y && y < (int32_t)(window->y + window->height)) {
            spinlock_release(&wm_state.lock);
            return window;
        }
        window = window->next;
    }
    
    spinlock_release(&wm_state.lock);
    return NULL;
}

/**
 * Render a window to framebuffer
 */
error_code_t window_render(window_t* window) {
    if (!window || !(window->flags & WINDOW_FLAG_VISIBLE)) {
        return ERR_INVALID_ARG;
    }
    
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    // Clip to framebuffer bounds
    int32_t src_x = 0;
    int32_t src_y = 0;
    int32_t dst_x = window->x;
    int32_t dst_y = window->y;
    uint32_t width = window->width;
    uint32_t height = window->height;
    
    if (dst_x < 0) {
        src_x = -dst_x;
        width -= src_x;
        dst_x = 0;
    }
    if (dst_y < 0) {
        src_y = -dst_y;
        height -= src_y;
        dst_y = 0;
    }
    if (dst_x + width > fb->width) {
        width = fb->width - dst_x;
    }
    if (dst_y + height > fb->height) {
        height = fb->height - dst_y;
    }
    
    if (width == 0 || height == 0) {
        return ERR_OK;  // Window outside framebuffer
    }
    
    // Copy window buffer to framebuffer
    uint32_t* src = window->buffer + src_y * window->width + src_x;
    uint8_t* dst = (uint8_t*)fb->base_address + dst_y * fb->pitch + dst_x * (fb->bpp / 8);
    
    for (uint32_t y = 0; y < height; y++) {
        if (fb->bpp == 32) {
            memcpy(dst, src, width * 4);
        } else {
            // Convert pixel by pixel for other formats
            uint32_t* src_row = src;
            uint32_t* dst_row = (uint32_t*)dst;
            for (uint32_t x = 0; x < width; x++) {
                framebuffer_set_pixel(dst_x + x, dst_y + y, src_row[x]);
            }
        }
        src += window->width;
        dst += fb->pitch;
    }
    
    // Draw window border if focused
    if (window->flags & WINDOW_FLAG_FOCUSED) {
        gfx_draw_rect(window->x, window->y, window->width, window->height, RGB(0, 120, 215));
    } else {
        gfx_draw_rect(window->x, window->y, window->width, window->height, RGB(200, 200, 200));
    }
    
    // Draw title bar
    if (window->title[0]) {
        gfx_fill_rect(window->x, window->y, window->width, 20, RGB(50, 50, 50));
        gfx_draw_string(window->x + 5, window->y + 12, window->title, RGB(255, 255, 255), 0xFFFFFFFF);
    }
    
    // Render widgets if any
    if (window->widget_root) {
        extern error_code_t widget_render(void* widget, window_t* window);
        widget_render((void*)window->widget_root, window);
    }
    
    return ERR_OK;
}

/**
 * Render all windows
 */
error_code_t window_manager_render_all(void) {
    if (!wm_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    framebuffer_t* fb = framebuffer_get();
    if (!fb) {
        return ERR_DEVICE_NOT_FOUND;
    }
    
    // Don't clear framebuffer here - desktop_render() handles the background
    // framebuffer_clear(RGB(50, 50, 50));  // Dark gray background
    
    // Render all windows (back to front)
    spinlock_acquire(&wm_state.lock);
    window_t* window = wm_state.windows;
    while (window) {
        if (window->flags & WINDOW_FLAG_VISIBLE) {
            window_render(window);
        }
        window = window->next;
    }
    spinlock_release(&wm_state.lock);
    
    // Swap buffers if double buffering is enabled
    gfx_swap_buffers();
    
    return ERR_OK;
}

/**
 * Handle input events
 */
error_code_t window_manager_handle_input(void) {
    if (!wm_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    input_event_t event;
    while (input_event_available()) {
        if (input_event_dequeue(&event) != ERR_OK) {
            break;
        }
        
        switch (event.type) {
            case INPUT_EVENT_KEYBOARD: {
                key_event_t* key = &event.data.keyboard;
                window_t* focused = window_get_focused();
                
                if (focused && focused->widget_root && key->state == KEY_STATE_PRESSED) {
                    // Handle keyboard input for focused window's widgets
                    extern error_code_t widget_handle_keyboard(void* widget, uint8_t keycode, char ascii);
                    widget_handle_keyboard(focused->widget_root, key->scancode, key->ascii);
                }
                break;
            }
            
            case INPUT_EVENT_MOUSE: {
                mouse_event_t* mouse = &event.data.mouse;
                static int32_t mouse_x = 0;
                static int32_t mouse_y = 0;
                static bool mouse_buttons[3] = {false, false, false};
                
                // Update mouse position
                mouse_x += mouse->x;
                mouse_y += mouse->y;
                
                // Clamp to screen bounds
                framebuffer_t* fb = framebuffer_get();
                if (fb) {
                    if (mouse_x < 0) mouse_x = 0;
                    if (mouse_y < 0) mouse_y = 0;
                    if (mouse_x >= (int32_t)fb->width) mouse_x = fb->width - 1;
                    if (mouse_y >= (int32_t)fb->height) mouse_y = fb->height - 1;
                }
                
                // Handle button clicks
                if (mouse->button_left && !mouse_buttons[0]) {
                    // Left button pressed
                    window_t* window = window_find_at(mouse_x, mouse_y);
                    if (window) {
                        window_set_focus(window, true);
                        
                        // Check if click was on a widget
                        if (window->widget_root) {
                            int32_t rel_x = mouse_x - window->x;
                            int32_t rel_y = mouse_y - window->y - 20;  // Account for title bar
                            
                            extern error_code_t widget_handle_mouse(void* widget, int32_t x, int32_t y, bool clicked);
                            widget_handle_mouse(window->widget_root, rel_x, rel_y, true);
                        }
                    }
                    mouse_buttons[0] = true;
                } else if (!mouse->button_left && mouse_buttons[0]) {
                    // Left button released
                    mouse_buttons[0] = false;
                }
                
                // Draw mouse cursor (simple crosshair)
                // This is a placeholder - proper cursor rendering would be better
                break;
            }
            
            default:
                break;
        }
    }
    
    return ERR_OK;
}

