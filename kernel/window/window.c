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
    window->alpha = 240;                    // Slight transparency
    window->blur_radius = 4;                // Moderate blur
    window->corner_radius = 10;             // Modern rounded corners
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
    spinlock_lock(&wm_state.lock);
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
    spinlock_unlock(&wm_state.lock);
    
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
    
    spinlock_lock(&wm_state.lock);
    
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
    
    spinlock_unlock(&wm_state.lock);
    
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
    
    spinlock_lock(&wm_state.lock);
    
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
    
    spinlock_unlock(&wm_state.lock);
    
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
    spinlock_lock(&wm_state.lock);
    
    // Search from front to back (top to bottom in Z-order)
    window_t* window = wm_state.windows;
    while (window) {
        if ((window->flags & WINDOW_FLAG_VISIBLE) &&
            x >= window->x && x < (int32_t)(window->x + window->width) &&
            y >= window->y && y < (int32_t)(window->y + window->height)) {
            spinlock_unlock(&wm_state.lock);
            return window;
        }
        window = window->next;
    }
    
    spinlock_unlock(&wm_state.lock);
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

    // 1. Draw Drop Shadow (Depth)
    // We draw this *outside* the window bounds, so we might need to adjust clipping or logic if strict window bounds are enforced.
    // For now, we assume the window manager repaints the background or handles damage rects correctly.
    // Ideally, shadows are drawn by the compositor. Here we draw it as part of the window for simplicity.
    gfx_draw_shadow(window->x, window->y, window->width, window->height, window->corner_radius, 100);

    // 2. Apply Background Blur (Frosted Glass)
    // This reads the current framebuffer content (what's behind the window) and blurs it.
    // Note: This is expensive! In a real compositor, we'd optimize this.
    if (window->blur_radius > 0) {
        gfx_apply_blur_region(window->x, window->y, window->width, window->height, window->blur_radius);
    }

    // 3. Draw Window Body (Semi-transparent Rounded Rect)
    // We use the window's background color and alpha.
    gfx_fill_rounded_rect_alpha(window->x, window->y, window->width, window->height, 
                                window->corner_radius, window->bg_color, window->alpha);

    // 4. Draw Window Border (Subtle)
    uint32_t border_color = (window->flags & WINDOW_FLAG_FOCUSED) ? RGB(100, 100, 100) : RGB(150, 150, 150);
    gfx_draw_rounded_rect(window->x, window->y, window->width, window->height, 
                          window->corner_radius, border_color);

    // 5. Draw Title Bar
    if (window->title[0]) {
        // Draw title bar background (slightly darker/different alpha)
        // We only round the top corners
        uint32_t title_height = 24;
        // Hack: Draw a rounded rect for the top part, and a normal rect for the bottom part of the title bar to flatten bottom corners
        // Or just draw a rounded rect and clip? 
        // Simpler: Just draw a rounded rect for the whole title bar area, but we need to handle the bottom flat edge.
        // Let's just draw a rounded rect for the top, and fill the bottom half to square it off.
        
        uint32_t title_bg = RGB(30, 30, 30);
        uint8_t title_alpha = 200;
        
        // Top rounded part
        gfx_fill_rounded_rect_alpha(window->x, window->y, window->width, title_height + window->corner_radius, 
                                    window->corner_radius, title_bg, title_alpha);
        
        // Square off the bottom of the title bar (so it connects to the window body)
        // Actually, the window body is already drawn. We just want the title bar to sit on top.
        // If we want the title bar to be distinct:
        // gfx_fill_rect_alpha(window->x, window->y + window->corner_radius, window->width, title_height - window->corner_radius, title_bg, title_alpha);
        
        // Draw Title Text
        // Center the text vertically
        gfx_draw_string(window->x + 10, window->y + 8, window->title, RGB(255, 255, 255), 0); // 0 = transparent bg for text
    }
    
    // 6. Render Widgets
    // Widgets need to be aware of the window's alpha/buffer? 
    // For now, we assume widgets draw directly to the framebuffer on top of the window.
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
    spinlock_lock(&wm_state.lock);
    window_t* window = wm_state.windows;
    while (window) {
        if (window->flags & WINDOW_FLAG_VISIBLE) {
            window_render(window);
        }
        window = window->next;
    }
    spinlock_unlock(&wm_state.lock);
    
    // Note: Don't swap buffers here - main loop handles it to avoid double-swapping
    
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

