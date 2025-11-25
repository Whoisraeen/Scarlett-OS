/**
 * @file compositor.h
 * @brief Crashless Window Compositor Implementation
 */

#ifndef GUI_COMPOSITOR_H
#define GUI_COMPOSITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "../../../libs/libgui/include/compositor_ipc.h" // For message types

// Maximum windows
#define MAX_WINDOWS 256

// Window states
typedef enum {
    WINDOW_STATE_HIDDEN = 0,
    WINDOW_STATE_NORMAL = 1,
    WINDOW_STATE_MINIMIZED = 2,
    WINDOW_STATE_MAXIMIZED = 3,
    WINDOW_STATE_FULLSCREEN = 4,
} window_state_t;

// Window flags
typedef enum {
    WINDOW_FLAG_DECORATED = 0x01,
    WINDOW_FLAG_RESIZABLE = 0x02,
    WINDOW_FLAG_MODAL = 0x04,
    WINDOW_FLAG_ALWAYS_ON_TOP = 0x08,
    WINDOW_FLAG_TRANSPARENT = 0x10,
} window_flags_t;

// Window structure
typedef struct {
    uint32_t id;
    uint32_t owner_pid;
    int32_t x, y;
    uint32_t width, height;
    window_state_t state;
    uint32_t flags;
    char title[128];
    void* framebuffer;      // Window's pixel buffer (mapped SHM)
    void* texture;          // UGAL texture for window (created from framebuffer)
    uint32_t shm_id;       // Shared memory ID for framebuffer
    uint32_t framebuffer_size; // Size of the framebuffer in bytes
    uint32_t z_order;
    bool dirty;             // Needs redraw
    bool visible;
    uint64_t client_ipc_port; // Port of the client owning this window
} window_t;

// Compositor state (for checkpointing)
typedef struct {
    window_t windows[MAX_WINDOWS];
    uint32_t window_count;
    uint32_t focused_window;
    uint32_t next_window_id;
    uint64_t checkpoint_time;
    uint32_t checkpoint_version;
} compositor_state_t;

// Compositor context
typedef struct {
    compositor_state_t* state;
    void* gpu_device;       // UGAL device
    void* screen_fb;        // Screen framebuffer
    uint32_t screen_width;
    uint32_t screen_height;
    uint32_t mouse_x, mouse_y;
    bool running;
    // Window dragging state
    bool dragging;
    uint32_t drag_window;
    int32_t drag_offset_x, drag_offset_y;
} compositor_ctx_t;

// Compositor operations
compositor_ctx_t* compositor_create(uint32_t width, uint32_t height);
void compositor_destroy(compositor_ctx_t* ctx);
void compositor_run(compositor_ctx_t* ctx);

// Window management
uint32_t compositor_create_window(compositor_ctx_t* ctx, uint32_t pid, int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t shm_id, const char* title, uint64_t client_ipc_port);
void compositor_destroy_window(compositor_ctx_t* ctx, uint32_t window_id);
void compositor_move_window(compositor_ctx_t* ctx, uint32_t window_id, int32_t x, int32_t y);
void compositor_resize_window(compositor_ctx_t* ctx, uint32_t window_id, uint32_t width, uint32_t height, uint32_t new_shm_id);
void compositor_set_window_state(compositor_ctx_t* ctx, uint32_t window_id, window_state_t state);
void compositor_set_window_title(compositor_ctx_t* ctx, uint32_t window_id, const char* title);
void compositor_raise_window(compositor_ctx_t* ctx, uint32_t window_id);
void compositor_focus_window(compositor_ctx_t* ctx, uint32_t window_id);

// Rendering
void compositor_render(compositor_ctx_t* ctx);
void compositor_damage_window(compositor_ctx_t* ctx, uint32_t window_id, int32_t x, int32_t y, uint32_t width, uint32_t height);

// State management (crash recovery)
void compositor_checkpoint(compositor_ctx_t* ctx);
bool compositor_restore(compositor_ctx_t* ctx);

// Input handling
void compositor_handle_mouse_move(compositor_ctx_t* ctx, int32_t x, int32_t y);
void compositor_handle_mouse_button(compositor_ctx_t* ctx, uint32_t button, bool pressed);
void compositor_handle_key(compositor_ctx_t* ctx, uint32_t keycode, bool pressed);


#endif // GUI_COMPOSITOR_H