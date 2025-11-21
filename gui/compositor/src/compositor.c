/**
 * @file compositor.c
 * @brief Crashless Window Compositor Implementation
 */

#include "compositor.h"
#include "../ugal/src/ugal.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// Checkpoint file path
#define CHECKPOINT_PATH "/var/compositor/state.checkpoint"

// Syscall numbers (from kernel/include/syscall/syscall.h)
#define SYS_OPEN  3
#define SYS_CLOSE 4
#define SYS_READ  2
#define SYS_WRITE 1
#define SYS_MKDIR 20  // Approximate, may need adjustment

// VFS flags (from kernel/include/fs/vfs.h)
#define VFS_MODE_READ   (1 << 0)
#define VFS_MODE_WRITE  (1 << 1)
#define VFS_MODE_CREATE (1 << 3)
#define VFS_MODE_TRUNC  (1 << 5)

// Syscall wrapper (x86_64)
static inline uint64_t syscall_raw(uint64_t num, uint64_t arg1, uint64_t arg2,
                                   uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    uint64_t ret;
    #if defined(__x86_64__)
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r10"(arg4), "r8"(arg5)
        : "rcx", "r11", "memory"
    );
    #else
    // Fallback for other architectures
    ret = 0;
    #endif
    return ret;
}

// File operation wrappers
static int sys_open(const char* path, uint64_t flags) {
    return (int)syscall_raw(SYS_OPEN, (uint64_t)path, flags, 0, 0, 0);
}

static int sys_close(int fd) {
    return (int)syscall_raw(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
}

static ssize_t sys_read(int fd, void* buf, size_t count) {
    return (ssize_t)syscall_raw(SYS_READ, (uint64_t)fd, (uint64_t)buf, count, 0, 0);
}

static ssize_t sys_write(int fd, const void* buf, size_t count) {
    return (ssize_t)syscall_raw(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, count, 0, 0);
}

// Create compositor
compositor_ctx_t* compositor_create(uint32_t width, uint32_t height) {
    compositor_ctx_t* ctx = (compositor_ctx_t*)malloc(sizeof(compositor_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(compositor_ctx_t));

    // Allocate state
    ctx->state = (compositor_state_t*)malloc(sizeof(compositor_state_t));
    if (!ctx->state) {
        free(ctx);
        return NULL;
    }

    memset(ctx->state, 0, sizeof(compositor_state_t));
    ctx->state->next_window_id = 1;

    // Initialize screen
    ctx->screen_width = width;
    ctx->screen_height = height;

    // Initialize UGAL GPU device
    ctx->gpu_device = ugal_create_device(0);
    if (!ctx->gpu_device) {
        free(ctx->state);
        free(ctx);
        return NULL;
    }

    // Create screen framebuffer
    ctx->screen_fb = ugal_create_framebuffer(ctx->gpu_device, width, height);

    ctx->running = true;

    // Try to restore from checkpoint
    bool restored = compositor_restore(ctx);
    if (!restored) {
        // No checkpoint, start fresh
        ctx->state->checkpoint_version = 1;
        ctx->state->checkpoint_time = 0;  // TODO: Get actual time
    } else {
        // Successfully restored from checkpoint
        // Mark all windows as dirty so they get redrawn
        for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
            if (ctx->state->windows[i].id != 0) {
                ctx->state->windows[i].dirty = true;
            }
        }
    }

    return ctx;
}

// Destroy compositor
void compositor_destroy(compositor_ctx_t* ctx) {
    if (!ctx) return;

    // Checkpoint state before exit
    compositor_checkpoint(ctx);

    // Cleanup GPU resources
    if (ctx->screen_fb) {
        ugal_destroy_framebuffer(ctx->screen_fb);
    }

    if (ctx->gpu_device) {
        ugal_destroy_device(ctx->gpu_device);
    }

    free(ctx->state);
    free(ctx);
}

// Create window
uint32_t compositor_create_window(compositor_ctx_t* ctx, uint32_t pid, int32_t x, int32_t y, uint32_t width, uint32_t height, const char* title) {
    if (!ctx || ctx->state->window_count >= MAX_WINDOWS) {
        return 0;
    }

    // Find free window slot
    for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
        if (ctx->state->windows[i].id == 0) {
            window_t* win = &ctx->state->windows[i];

            win->id = ctx->state->next_window_id++;
            win->owner_pid = pid;
            win->x = x;
            win->y = y;
            win->width = width;
            win->height = height;
            win->state = WINDOW_STATE_NORMAL;
            win->flags = WINDOW_FLAG_DECORATED | WINDOW_FLAG_RESIZABLE;
            win->z_order = ctx->state->window_count;
            win->dirty = true;
            win->visible = true;

            if (title) {
                strncpy(win->title, title, 127);
                win->title[127] = '\0';
            }

            // Allocate framebuffer for window
            // TODO: Use shared memory with application

            ctx->state->window_count++;

            // Checkpoint after window creation
            compositor_checkpoint(ctx);

            return win->id;
        }
    }

    return 0;
}

// Destroy window
void compositor_destroy_window(compositor_ctx_t* ctx, uint32_t window_id) {
    if (!ctx) return;

    for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
        if (ctx->state->windows[i].id == window_id) {
            // Free window framebuffer
            // TODO: Cleanup shared memory

            memset(&ctx->state->windows[i], 0, sizeof(window_t));
            ctx->state->window_count--;

            // Checkpoint after window destruction
            compositor_checkpoint(ctx);

            // Mark screen dirty
            compositor_render(ctx);

            break;
        }
    }
}

// Move window
void compositor_move_window(compositor_ctx_t* ctx, uint32_t window_id, int32_t x, int32_t y) {
    if (!ctx) return;

    for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
        if (ctx->state->windows[i].id == window_id) {
            ctx->state->windows[i].x = x;
            ctx->state->windows[i].y = y;
            ctx->state->windows[i].dirty = true;

            // Checkpoint periodically (not on every move)
            break;
        }
    }
}

// Resize window
void compositor_resize_window(compositor_ctx_t* ctx, uint32_t window_id, uint32_t width, uint32_t height) {
    if (!ctx) return;

    for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
        if (ctx->state->windows[i].id == window_id) {
            ctx->state->windows[i].width = width;
            ctx->state->windows[i].height = height;
            ctx->state->windows[i].dirty = true;

            // TODO: Reallocate framebuffer

            compositor_checkpoint(ctx);
            break;
        }
    }
}

// Raise window (bring to front)
void compositor_raise_window(compositor_ctx_t* ctx, uint32_t window_id) {
    if (!ctx) return;

    uint32_t window_idx = MAX_WINDOWS;
    for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
        if (ctx->state->windows[i].id == window_id) {
            window_idx = i;
            break;
        }
    }

    if (window_idx == MAX_WINDOWS) return;

    // Find max z-order
    uint32_t max_z = 0;
    for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
        if (ctx->state->windows[i].id != 0 && ctx->state->windows[i].z_order > max_z) {
            max_z = ctx->state->windows[i].z_order;
        }
    }

    ctx->state->windows[window_idx].z_order = max_z + 1;
    ctx->state->windows[window_idx].dirty = true;
}

// Focus window
void compositor_focus_window(compositor_ctx_t* ctx, uint32_t window_id) {
    if (!ctx) return;

    ctx->state->focused_window = window_id;
    compositor_raise_window(ctx, window_id);
}

// Render all windows to screen
void compositor_render(compositor_ctx_t* ctx) {
    if (!ctx || !ctx->gpu_device || !ctx->screen_fb) return;

    // Clear screen
    ugal_clear(ctx->gpu_device, ctx->screen_fb, 0xFF204060); // Dark blue background

    // Sort windows by z-order
    // TODO: Implement proper sorting

    // Render each window
    for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
        window_t* win = &ctx->state->windows[i];

        if (win->id == 0 || !win->visible) continue;

        // TODO: Blit window framebuffer to screen
        // ugal_blit(ctx->gpu_device, win->framebuffer, ctx->screen_fb, ...);

        // Draw window decorations if needed
        if (win->flags & WINDOW_FLAG_DECORATED) {
            // TODO: Draw title bar, borders, buttons
        }

        win->dirty = false;
    }

    // Present to display
    ugal_present(ctx->gpu_device, ctx->screen_fb);
}

// Checkpoint compositor state
void compositor_checkpoint(compositor_ctx_t* ctx) {
    if (!ctx || !ctx->state) return;

    // Update checkpoint version
    ctx->state->checkpoint_version++;
    // TODO: Update checkpoint_time with actual time when time API is available

    // Open checkpoint file for writing
    int fd = sys_open(CHECKPOINT_PATH, VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC);
    if (fd < 0) {
        // Failed to open file - checkpoint directory might not exist
        // This is not fatal, just log and continue
        return;
    }

    // Write state structure to file
    size_t state_size = sizeof(compositor_state_t);
    ssize_t written = sys_write(fd, ctx->state, state_size);
    
    if (written != (ssize_t)state_size) {
        // Write failed or incomplete
        sys_close(fd);
        return;
    }

    // Close file
    sys_close(fd);
}

// Restore compositor state from checkpoint
bool compositor_restore(compositor_ctx_t* ctx) {
    if (!ctx || !ctx->state) return false;

    // Try to open checkpoint file for reading
    int fd = sys_open(CHECKPOINT_PATH, VFS_MODE_READ);
    if (fd < 0) {
        // No checkpoint file exists - start fresh
        return false;
    }

    // Read state structure from file
    size_t state_size = sizeof(compositor_state_t);
    ssize_t read_bytes = sys_read(fd, ctx->state, state_size);
    
    sys_close(fd);

    if (read_bytes != (ssize_t)state_size) {
        // Read failed or incomplete - invalid checkpoint
        memset(ctx->state, 0, sizeof(compositor_state_t));
        ctx->state->next_window_id = 1;
        return false;
    }

    // Validate restored state
    if (ctx->state->window_count > MAX_WINDOWS) {
        // Invalid state - reset
        memset(ctx->state, 0, sizeof(compositor_state_t));
        ctx->state->next_window_id = 1;
        return false;
    }

    // State restored successfully
    // Note: Window framebuffers will need to be re-established by applications
    // The compositor just restores window positions, sizes, and metadata
    
    return true;
}

// Main compositor loop
void compositor_run(compositor_ctx_t* ctx) {
    if (!ctx) return;

    while (ctx->running) {
        // Process IPC messages from applications
        // TODO: Implement IPC message handling

        // Render if any windows are dirty
        bool needs_render = false;
        for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
            if (ctx->state->windows[i].id != 0 && ctx->state->windows[i].dirty) {
                needs_render = true;
                break;
            }
        }

        if (needs_render) {
            compositor_render(ctx);
        }

        // Checkpoint periodically (e.g., every 100 frames)
        static uint32_t frame_count = 0;
        if (++frame_count >= 100) {
            compositor_checkpoint(ctx);
            frame_count = 0;
        }

        // Yield CPU
        syscall_raw(6, 0, 0, 0, 0, 0);  // SYS_YIELD = 6
    }
}

// Handle mouse move
void compositor_handle_mouse_move(compositor_ctx_t* ctx, int32_t x, int32_t y) {
    if (!ctx) return;

    ctx->mouse_x = x;
    ctx->mouse_y = y;

    // TODO: Update cursor, check for window drag, etc.
}

// Handle mouse button
void compositor_handle_mouse_button(compositor_ctx_t* ctx, uint32_t button, bool pressed) {
    if (!ctx) return;

    if (pressed) {
        // Find window under mouse
        for (int i = MAX_WINDOWS - 1; i >= 0; i--) {
            window_t* win = &ctx->state->windows[i];

            if (win->id == 0 || !win->visible) continue;

            if (ctx->mouse_x >= win->x && ctx->mouse_x < win->x + (int32_t)win->width &&
                ctx->mouse_y >= win->y && ctx->mouse_y < win->y + (int32_t)win->height) {

                // Focus this window
                compositor_focus_window(ctx, win->id);

                // TODO: Send mouse event to application

                break;
            }
        }
    }
}

// Handle keyboard input
void compositor_handle_key(compositor_ctx_t* ctx, uint32_t keycode, bool pressed) {
    if (!ctx) return;

    // Send key event to focused window
    if (ctx->state->focused_window != 0) {
        // TODO: Send key event via IPC to application
    }
}
