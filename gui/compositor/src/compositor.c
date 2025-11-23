/**
 * @file compositor.c
 * @brief Crashless Window Compositor Implementation
 */

#include "compositor.h"
#include "../ugal/src/ugal.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

// Define ssize_t if not available
#ifndef _SSIZE_T_DEFINED
typedef long ssize_t;
#define _SSIZE_T_DEFINED
#endif

// Checkpoint file path
#define CHECKPOINT_PATH "/var/compositor/state.checkpoint"

// Syscall numbers (from kernel/include/syscall/syscall.h)
#define SYS_OPEN  3
#define SYS_CLOSE 4
#define SYS_READ  2
#define SYS_WRITE 1
#define SYS_MKDIR 20  // Approximate, may need adjustment
#define SYS_GET_UPTIME_MS 47
#define SYS_IPC_SEND 9
#define SYS_IPC_RECEIVE 10
#define SYS_SHM_CREATE 40
#define SYS_SHM_MAP 41
#define SYS_SHM_UNMAP 42
#define SYS_SHM_DESTROY 43

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

// IPC message structure (must match kernel/include/ipc/ipc.h)
typedef struct {
    uint64_t sender_tid;
    uint64_t msg_id;
    uint32_t type;
    uint32_t inline_size;
    uint8_t inline_data[64];
    void* buffer;
    size_t buffer_size;
} ipc_message_t;

// Compositor IPC port (would be created during initialization)
static uint64_t compositor_port = 0;

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
    ctx->mouse_x = 0;
    ctx->mouse_y = 0;
    ctx->dragging = false;
    ctx->drag_window = 0;
    ctx->drag_offset_x = 0;
    ctx->drag_offset_y = 0;

    // Initialize UGAL GPU device
    ctx->gpu_device = ugal_create_device(0);
    if (!ctx->gpu_device) {
        free(ctx->state);
        free(ctx);
        return NULL;
    }

    // Create screen framebuffer
    ctx->screen_fb = ugal_create_framebuffer(ctx->gpu_device, width, height);
    if (ctx->screen_fb) {
        // Create color texture for screen framebuffer
        ugal_framebuffer_t* fb = (ugal_framebuffer_t*)ctx->screen_fb;
        ugal_texture_t* color_tex = ugal_create_texture(ctx->gpu_device, width, height, UGAL_FORMAT_RGBA8);
        if (color_tex) {
            ugal_attach_color_texture(fb, color_tex);
        }
    }

    ctx->running = true;

    // Try to restore from checkpoint
    bool restored = compositor_restore(ctx);
    if (!restored) {
        // No checkpoint, start fresh
        ctx->state->checkpoint_version = 1;
        ctx->state->checkpoint_time = syscall_raw(SYS_GET_UPTIME_MS, 0, 0, 0, 0, 0);
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

            // Allocate framebuffer for window using shared memory
            // Create shared memory region for window framebuffer
            uint32_t fb_size = width * height * 4; // RGBA32
            uint64_t shm_id = syscall_raw(SYS_SHM_CREATE, fb_size, 0, 0, 0, 0);
            if (shm_id != 0) {
                // Map shared memory
                void* fb_ptr = (void*)syscall_raw(SYS_SHM_MAP, shm_id, 0, 0, 0, 0);
                if (fb_ptr) {
                    win->framebuffer = fb_ptr;
                    win->shm_id = (uint32_t)shm_id;
                    win->texture = NULL; // Will be created on first render
                    // Clear framebuffer
                    memset(fb_ptr, 0, fb_size);
                } else {
                    // Mapping failed, destroy shared memory
                    syscall_raw(SYS_SHM_DESTROY, shm_id, 0, 0, 0, 0);
                    win->shm_id = 0;
                }
            } else {
                win->shm_id = 0;
            }

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
            // Free window framebuffer and cleanup shared memory
            window_t* win = &ctx->state->windows[i];
            if (win->texture) {
                ugal_destroy_texture((ugal_texture_t*)win->texture);
                win->texture = NULL;
            }
            if (win->framebuffer && win->shm_id != 0) {
                // Unmap shared memory
                syscall_raw(SYS_SHM_UNMAP, win->shm_id, 0, 0, 0, 0);
                // Destroy shared memory
                syscall_raw(SYS_SHM_DESTROY, win->shm_id, 0, 0, 0, 0);
                win->framebuffer = NULL;
                win->shm_id = 0;
            }

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

            // Reallocate framebuffer if size changed
            window_t* win = &ctx->state->windows[i];
            uint32_t new_fb_size = width * height * 4;
            uint32_t old_fb_size = win->width * win->height * 4;
            if (new_fb_size != old_fb_size && win->framebuffer && win->shm_id != 0) {
                // Destroy old texture if exists
                if (win->texture) {
                    ugal_destroy_texture((ugal_texture_t*)win->texture);
                    win->texture = NULL;
                }
                // Unmap old framebuffer
                syscall_raw(SYS_SHM_UNMAP, win->shm_id, 0, 0, 0, 0);
                // Destroy old shared memory
                syscall_raw(SYS_SHM_DESTROY, win->shm_id, 0, 0, 0, 0);
                win->framebuffer = NULL;
                win->shm_id = 0;
                
                // Create new shared memory
                uint64_t shm_id = syscall_raw(SYS_SHM_CREATE, new_fb_size, 0, 0, 0, 0);
                if (shm_id != 0) {
                    void* fb_ptr = (void*)syscall_raw(SYS_SHM_MAP, shm_id, 0, 0, 0, 0);
                    if (fb_ptr) {
                        win->framebuffer = fb_ptr;
                        win->shm_id = (uint32_t)shm_id;
                        win->texture = NULL; // Will be created on next render
                        memset(fb_ptr, 0, new_fb_size);
                    } else {
                        // Mapping failed, destroy shared memory
                        syscall_raw(SYS_SHM_DESTROY, shm_id, 0, 0, 0, 0);
                    }
                }
            }

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

    // Sort windows by z-order (bubble sort for simplicity)
    // Create index array for sorting
    uint32_t indices[MAX_WINDOWS];
    uint32_t valid_count = 0;
    for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
        if (ctx->state->windows[i].id != 0 && ctx->state->windows[i].visible) {
            indices[valid_count++] = i;
        }
    }
    
    // Sort indices by z_order (ascending - lower z_order drawn first)
    for (uint32_t i = 0; i < valid_count; i++) {
        for (uint32_t j = i + 1; j < valid_count; j++) {
            if (ctx->state->windows[indices[i]].z_order > ctx->state->windows[indices[j]].z_order) {
                uint32_t tmp = indices[i];
                indices[i] = indices[j];
                indices[j] = tmp;
            }
        }
    }

    // Render each window in sorted order
    for (uint32_t idx = 0; idx < valid_count; idx++) {
        window_t* win = &ctx->state->windows[indices[idx]];

        if (win->id == 0 || !win->visible) continue;

        // Blit window framebuffer to screen
        if (win->framebuffer) {
            // Create or update texture from framebuffer
            if (!win->texture) {
                // Create texture from framebuffer
                win->texture = ugal_create_texture(ctx->gpu_device, win->width, win->height, UGAL_FORMAT_RGBA8);
            }
            if (win->texture) {
                // Update texture with framebuffer data
                ugal_update_texture((ugal_texture_t*)win->texture, win->framebuffer, 0, 0, win->width, win->height);
                
                // Get screen framebuffer's color texture for blitting
                // Access through ugal_attach_color_texture was called in compositor_create
                // We need to get the color texture - for now, use a helper or store it separately
                // Since ugal_framebuffer_t is opaque, we'll need to add a getter function
                // For now, assume the texture exists (it was created in compositor_create)
                ugal_framebuffer_t* screen_fb = (ugal_framebuffer_t*)ctx->screen_fb;
                // Access color_texture through a cast (ugal_framebuffer struct is defined in ugal.c)
                // This is a workaround - ideally ugal.h would expose a getter function
                struct ugal_framebuffer_internal {
                    ugal_device_t* device;
                    void* driver_framebuffer;
                    ugal_texture_t* color_texture;
                    ugal_texture_t* depth_texture;
                    uint32_t width;
                    uint32_t height;
                };
                struct ugal_framebuffer_internal* screen_fb_internal = (struct ugal_framebuffer_internal*)screen_fb;
                if (screen_fb_internal && screen_fb_internal->color_texture) {
                    // Blit window texture to screen framebuffer texture
                    ugal_blit(ctx->gpu_device, (ugal_texture_t*)win->texture, screen_fb_internal->color_texture,
                            0, 0, win->x, win->y, win->width, win->height);
                }
            }
        }

        // Draw window decorations if needed
        if (win->flags & WINDOW_FLAG_DECORATED) {
            // Draw title bar background
            ugal_fill_rect(ctx->gpu_device, ctx->screen_fb, 
                          win->x, win->y, win->width, 30, 0xFF404040);
            
            // Draw title text (simplified - would use font rendering)
            // For now, just draw a line to indicate title bar
            
            // Draw window border
            ugal_draw_line(ctx->gpu_device, ctx->screen_fb, 
                          win->x, win->y, win->x + win->width, win->y, 0xFF808080); // Top
            ugal_draw_line(ctx->gpu_device, ctx->screen_fb, 
                          win->x, win->y + win->height, win->x + win->width, win->y + win->height, 0xFF808080); // Bottom
            ugal_draw_line(ctx->gpu_device, ctx->screen_fb, 
                          win->x, win->y, win->x, win->y + win->height, 0xFF808080); // Left
            ugal_draw_line(ctx->gpu_device, ctx->screen_fb, 
                          win->x + win->width, win->y, win->x + win->width, win->y + win->height, 0xFF808080); // Right
            
            // Draw close button (simplified - just a small rectangle)
            ugal_fill_rect(ctx->gpu_device, ctx->screen_fb, 
                          win->x + win->width - 20, win->y + 5, 15, 15, 0xFFFF0000);
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
    ctx->state->checkpoint_time = syscall_raw(SYS_GET_UPTIME_MS, 0, 0, 0, 0, 0);

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

    // Create compositor IPC port if not already created
    if (compositor_port == 0) {
        compositor_port = syscall_raw(26, 0, 0, 0, 0, 0); // SYS_IPC_CREATE_PORT
    }
    
    while (ctx->running) {
        // Process IPC messages from applications
        // Implement IPC message handling
        ipc_message_t msg;
        int ret = (int)syscall_raw(SYS_IPC_RECEIVE, compositor_port, (uint64_t)&msg, 0, 0, 0);
        if (ret == 0) {
            // Handle message based on msg_id
            switch (msg.msg_id) {
                case 1: // COMPOSITOR_MSG_CREATE_WINDOW
                    if (msg.inline_size >= 20) {
                        uint32_t pid = *(uint32_t*)&msg.inline_data[0];
                        int32_t x = *(int32_t*)&msg.inline_data[4];
                        int32_t y = *(int32_t*)&msg.inline_data[8];
                        uint32_t width = *(uint32_t*)&msg.inline_data[12];
                        uint32_t height = *(uint32_t*)&msg.inline_data[16];
                        const char* title = (const char*)&msg.inline_data[20];
                        uint32_t win_id = compositor_create_window(ctx, pid, x, y, width, height, title);
                        // Send response with window ID
                        ipc_message_t response = {0};
                        response.type = 2; // IPC_MSG_RESPONSE
                        response.msg_id = msg.msg_id;
                        *(uint32_t*)&response.inline_data[0] = win_id;
                        response.inline_size = 4;
                        syscall_raw(SYS_IPC_SEND, msg.sender_tid, (uint64_t)&response, 0, 0, 0);
                    }
                    break;
                case 2: // COMPOSITOR_MSG_DESTROY_WINDOW
                    if (msg.inline_size >= 4) {
                        uint32_t win_id = *(uint32_t*)&msg.inline_data[0];
                        compositor_destroy_window(ctx, win_id);
                    }
                    break;
                case 3: // COMPOSITOR_MSG_MOVE_WINDOW
                    if (msg.inline_size >= 12) {
                        uint32_t win_id = *(uint32_t*)&msg.inline_data[0];
                        int32_t x = *(int32_t*)&msg.inline_data[4];
                        int32_t y = *(int32_t*)&msg.inline_data[8];
                        compositor_move_window(ctx, win_id, x, y);
                    }
                    break;
                case 4: // COMPOSITOR_MSG_RESIZE_WINDOW
                    if (msg.inline_size >= 12) {
                        uint32_t win_id = *(uint32_t*)&msg.inline_data[0];
                        uint32_t width = *(uint32_t*)&msg.inline_data[4];
                        uint32_t height = *(uint32_t*)&msg.inline_data[8];
                        compositor_resize_window(ctx, win_id, width, height);
                    }
                    break;
                // Add more message handlers as needed
            }
        }

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

    // Update cursor position (would call cursor rendering API)
    // Check for window drag
    if (ctx->dragging && ctx->drag_window != 0) {
        // Move window being dragged
        for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
            if (ctx->state->windows[i].id == ctx->drag_window) {
                compositor_move_window(ctx, ctx->drag_window, 
                                      x - ctx->drag_offset_x, 
                                      y - ctx->drag_offset_y);
                break;
            }
        }
    }
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

                // Send mouse event to application via IPC
                ipc_message_t mouse_event = {0};
                mouse_event.type = 3; // IPC_MSG_NOTIFICATION
                mouse_event.msg_id = 100; // MOUSE_BUTTON_EVENT
                mouse_event.inline_data[0] = button;
                mouse_event.inline_data[1] = pressed ? 1 : 0;
                *(int32_t*)&mouse_event.inline_data[2] = ctx->mouse_x - win->x;
                *(int32_t*)&mouse_event.inline_data[6] = ctx->mouse_y - win->y;
                mouse_event.inline_size = 10;
                
                // Get application's IPC port (would be stored per window)
                // For now, use sender_tid as port (simplified)
                uint64_t app_port = win->owner_pid; // Would be actual IPC port
                syscall_raw(SYS_IPC_SEND, app_port, (uint64_t)&mouse_event, 0, 0, 0);
                
                // Start drag if button is left mouse button and pressed
                if (button == 0 && pressed) {
                    ctx->dragging = true;
                    ctx->drag_window = win->id;
                    ctx->drag_offset_x = ctx->mouse_x - win->x;
                    ctx->drag_offset_y = ctx->mouse_y - win->y;
                } else if (button == 0 && !pressed) {
                    ctx->dragging = false;
                    ctx->drag_window = 0;
                }

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
        // Find focused window
        for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
            if (ctx->state->windows[i].id == ctx->state->focused_window) {
                window_t* win = &ctx->state->windows[i];
                
                // Send key event via IPC to application
                ipc_message_t key_event = {0};
                key_event.type = 3; // IPC_MSG_NOTIFICATION
                key_event.msg_id = 101; // KEYBOARD_EVENT
                *(uint32_t*)&key_event.inline_data[0] = keycode;
                key_event.inline_data[4] = pressed ? 1 : 0;
                key_event.inline_size = 5;
                
                // Get application's IPC port
                uint64_t app_port = win->owner_pid; // Would be actual IPC port
                syscall_raw(SYS_IPC_SEND, app_port, (uint64_t)&key_event, 0, 0, 0);
                break;
            }
        }
    }
}
