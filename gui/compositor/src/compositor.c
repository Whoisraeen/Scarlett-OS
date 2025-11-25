/**
 * @file compositor.c
 * @brief Crashless Window Compositor Implementation
 */

#include "compositor.h"
#include "../ugal/src/ugal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h> // For snprintf

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
#define SYS_MKDIR 19 // SYS_GETCWD is 19, let's assume MKDIR is 20 for now
#define SYS_GET_UPTIME_MS 47
#define SYS_IPC_SEND 9
#define SYS_IPC_RECEIVE 10
#define SYS_SHM_CREATE 40
#define SYS_SHM_MAP 41
#define SYS_SHM_UNMAP 42
#define SYS_SHM_DESTROY 43
#define SYS_GETPID 13 // Required for owner_pid

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
    void* buffer; // Not used for this IPC, shared memory for framebuffer
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

// Simple 8x8 font for rendering text on compositor
static const uint8_t font8x8_basic[128][8] = {
#include "../../../apps/desktop/font8x8_basic.h" // Include font data
};

// Draw char function for compositor
static void draw_char_compositor(uint32_t* buffer, uint32_t width, int x, int y, char c, uint32_t color) {
    if (c < 0 || c > 127) return; // Only ASCII
    for (int dy = 0; dy < 8; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            if ((font8x8_basic[(int)c][dy] >> dx) & 1) {
                if (x + dx >= 0 && x + dx < (int)width && y + dy >= 0 && y + dy < (int)1920) { // Limit y to a large value for now
                    buffer[(y + dy) * width + (x + dx)] = color;
                }
            }
        }
    }
}

// Draw string function for compositor
static void draw_string_compositor(uint32_t* buffer, uint32_t width, int x, int y, const char* str, uint32_t color) {
    int cx = x;
    while (*str) {
        draw_char_compositor(buffer, width, cx, y, *str, color);
        cx += 8; // Monospace font width
        str++;
    }
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
        ugal_texture_t* color_tex = ugal_create_texture(ctx->gpu_device, width, height, UGAL_FORMAT_RGBA8);
        if (color_tex) {
            ugal_attach_color_texture((ugal_framebuffer_t*)ctx->screen_fb, color_tex);
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
        ugal_destroy_framebuffer((ugal_framebuffer_t*)ctx->screen_fb);
    }

    if (ctx->gpu_device) {
        ugal_destroy_device((ugal_device_t*)ctx->gpu_device);
    }

    free(ctx->state);
    free(ctx);
}

// Create window
uint32_t compositor_create_window(compositor_ctx_t* ctx, uint32_t pid, int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t shm_id, const char* title, uint64_t client_ipc_port) {
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
            win->shm_id = shm_id;
            win->framebuffer_size = width * height * 4; // RGBA32
            win->client_ipc_port = client_ipc_port;

            if (title) {
                strncpy(win->title, title, 127);
                win->title[127] = '\0';
            }

            // Map shared memory for window framebuffer
            if (shm_id != 0) {
                void* fb_ptr = (void*)syscall_raw(SYS_SHM_MAP, shm_id, 0, 0, 0, 0);
                if (fb_ptr) {
                    win->framebuffer = fb_ptr;
                    win->texture = NULL; // Will be created on first render
                } else {
                    // Mapping failed
                    win->framebuffer = NULL;
                    win->shm_id = 0;
                }
            } else {
                win->framebuffer = NULL;
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
void compositor_resize_window(compositor_ctx_t* ctx, uint32_t window_id, uint32_t width, uint32_t height, uint32_t new_shm_id) {
    if (!ctx) return;

    for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
        if (ctx->state->windows[i].id == window_id) {
            window_t* win = &ctx->state->windows[i];

            // Clean up old shared memory and texture
            if (win->texture) {
                ugal_destroy_texture((ugal_texture_t*)win->texture);
                win->texture = NULL;
            }
            if (win->framebuffer && win->shm_id != 0) {
                syscall_raw(SYS_SHM_UNMAP, win->shm_id, 0, 0, 0, 0);
                syscall_raw(SYS_SHM_DESTROY, win->shm_id, 0, 0, 0, 0);
                win->framebuffer = NULL;
            }

            win->width = width;
            win->height = height;
            win->dirty = true;
            win->shm_id = new_shm_id;
            win->framebuffer_size = width * height * 4;

            // Map new shared memory
            if (new_shm_id != 0) {
                void* fb_ptr = (void*)syscall_raw(SYS_SHM_MAP, new_shm_id, 0, 0, 0, 0);
                if (fb_ptr) {
                    win->framebuffer = fb_ptr;
                } else {
                    win->framebuffer = NULL;
                    win->shm_id = 0;
                }
            } else {
                win->framebuffer = NULL;
            }

            compositor_checkpoint(ctx);
            break;
        }
    }
}


// Set window state (visible/hidden, etc.)
void compositor_set_window_state(compositor_ctx_t* ctx, uint32_t window_id, window_state_t state) {
    if (!ctx) return;

    for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
        if (ctx->state->windows[i].id == window_id) {
            ctx->state->windows[i].state = state;
            ctx->state->windows[i].visible = (state != WINDOW_STATE_HIDDEN);
            ctx->state->windows[i].dirty = true;
            break;
        }
    }
}

// Set window title
void compositor_set_window_title(compositor_ctx_t* ctx, uint32_t window_id, const char* title) {
    if (!ctx) return;

    for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
        if (ctx->state->windows[i].id == window_id) {
            strncpy(ctx->state->windows[i].title, title, 127);
            ctx->state->windows[i].title[127] = '\0';
            ctx->state->windows[i].dirty = true;
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
    ugal_clear(ctx->gpu_device, (ugal_framebuffer_t*)ctx->screen_fb, 0xFF204060); // Dark blue background

    // Sort windows by z-order (bubble sort for simplicity)
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
                win->texture = ugal_create_texture(ctx->gpu_device, win->width, win->height, UGAL_FORMAT_RGBA8);
            }
            if (win->texture) {
                // Update texture with framebuffer data from shared memory
                ugal_update_texture((ugal_texture_t*)win->texture, win->framebuffer, 0, 0, win->width, win->height);
                
                ugal_framebuffer_t* screen_fb = (ugal_framebuffer_t*)ctx->screen_fb;
                struct ugal_framebuffer_internal { // Access internal struct for color_texture
                    ugal_device_t* device;
                    void* driver_framebuffer;
                    ugal_texture_t* color_texture;
                    ugal_texture_t* depth_texture;
                    uint32_t width;
                    uint32_t height;
                };
                struct ugal_framebuffer_internal* screen_fb_internal = (struct ugal_framebuffer_internal*)screen_fb;
                if (screen_fb_internal && screen_fb_internal->color_texture) {
                    ugal_blit(ctx->gpu_device, (ugal_texture_t*)win->texture, screen_fb_internal->color_texture,
                            0, 0, win->x, win->y, win->width, win->height);
                }
            }
        }

        // Draw window decorations
        if (win->flags & WINDOW_FLAG_DECORATED) {
            // Draw title bar background
            ugal_fill_rect(ctx->gpu_device, (ugal_framebuffer_t*)ctx->screen_fb, 
                          win->x, win->y, win->width, 30, 0xFF404040);
            
            // Draw title text
            if (win->title[0] != '\0') {
                 draw_string_compositor((uint32_t*)((struct ugal_framebuffer_internal*)ctx->screen_fb)->driver_framebuffer,
                                        ctx->screen_width, win->x + 5, win->y + 8, win->title, 0xFFFFFFFF); // White text
            }
            
            // Draw window border
            ugal_draw_line(ctx->gpu_device, (ugal_framebuffer_t*)ctx->screen_fb, 
                          win->x, win->y, win->x + win->width, win->y, 0xFF808080); // Top
            ugal_draw_line(ctx->gpu_device, (ugal_framebuffer_t*)ctx->screen_fb, 
                          win->x, win->y + win->height, win->x + win->width, win->y + win->height, 0xFF808080); // Bottom
            ugal_draw_line(ctx->gpu_device, (ugal_framebuffer_t*)ctx->screen_fb, 
                          win->x, win->y, win->x, win->y + win->height, 0xFF808080); // Left
            ugal_draw_line(ctx->gpu_device, (ugal_framebuffer_t*)ctx->screen_fb, 
                          win->x + win->width, win->y, win->x + win->width, win->y + win->height, 0xFF808080); // Right
            
            // Draw close button (X)
            ugal_fill_rect(ctx->gpu_device, (ugal_framebuffer_t*)ctx->screen_fb, 
                          win->x + win->width - 25, win->y + 7, 20, 16, 0xFFFF0000); // Red background
            draw_string_compositor((uint32_t*)((struct ugal_framebuffer_internal*)ctx->screen_fb)->driver_framebuffer,
                                   ctx->screen_width, win->x + win->width - 20, win->y + 8, "X", 0xFFFFFFFF); // White 'X'
        }

        win->dirty = false;
    }

    // Present to display
    ugal_present(ctx->gpu_device, (ugal_framebuffer_t*)ctx->screen_fb);
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
    // Remap shared memory for existing windows
    for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
        window_t* win = &ctx->state->windows[i];
        if (win->id != 0 && win->shm_id != 0) {
            void* fb_ptr = (void*)syscall_raw(SYS_SHM_MAP, win->shm_id, 0, 0, 0, 0);
            if (fb_ptr) {
                win->framebuffer = fb_ptr;
            } else {
                win->framebuffer = NULL;
                win->shm_id = 0;
            }
        }
    }
    
    return true;
}

// Main compositor loop
void compositor_run(compositor_ctx_t* ctx) {
    if (!ctx) return;

    // Create compositor IPC port if not already created
    if (compositor_port == 0) {
        compositor_port = syscall_raw(26, 0, 0, 0, 0, 0); // SYS_IPC_CREATE_PORT
        
        // Publish port
        syscall_raw(SYS_MKDIR, (uint64_t)"/var", 0755, 0, 0, 0); // Ensure /var exists
        syscall_raw(SYS_MKDIR, (uint64_t)"/var/run", 0755, 0, 0, 0); // Ensure /var/run exists
        
        int fd = sys_open("/var/run/compositor.port", VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC);
        if (fd >= 0) {
            sys_write(fd, &compositor_port, sizeof(uint64_t));
            sys_close(fd);
        }
    }
    
    while (ctx->running) {
        // Process IPC messages from applications
        ipc_message_t msg;
        int ret = (int)syscall_raw(SYS_IPC_RECEIVE, compositor_port, (uint64_t)&msg, 0, 0, 0);
        if (ret == 0) {
            // Handle message based on msg_id
            switch (msg.msg_id) {
                case COMPOSITOR_MSG_CREATE_WINDOW: {
                    compositor_create_window_msg_t* create_msg = (compositor_create_window_msg_t*)msg.inline_data;
                    uint32_t win_id = compositor_create_window(ctx, create_msg->pid, create_msg->x, create_msg->y, 
                                                               create_msg->width, create_msg->height, create_msg->shm_id, 
                                                               create_msg->title, msg.sender_tid);
                    // Send response with window ID
                    ipc_message_t response = {0};
                    response.type = 2; // IPC_MSG_RESPONSE
                    response.msg_id = msg.msg_id;
                    *(uint32_t*)&response.inline_data[0] = win_id;
                    response.inline_size = 4;
                    syscall_raw(SYS_IPC_SEND, msg.sender_tid, (uint64_t)&response, 0, 0, 0);
                    break;
                }
                case COMPOSITOR_MSG_DESTROY_WINDOW: {
                    compositor_destroy_window_msg_t* destroy_msg = (compositor_destroy_window_msg_t*)msg.inline_data;
                    compositor_destroy_window(ctx, destroy_msg->window_id);
                    break;
                }
                case COMPOSITOR_MSG_MOVE_WINDOW: {
                    compositor_move_window_msg_t* move_msg = (compositor_move_window_msg_t*)msg.inline_data;
                    compositor_move_window(ctx, move_msg->window_id, move_msg->x, move_msg->y);
                    break;
                }
                case COMPOSITOR_MSG_RESIZE_WINDOW: {
                    compositor_resize_window_msg_t* resize_msg = (compositor_resize_window_msg_t*)msg.inline_data;
                    compositor_resize_window(ctx, resize_msg->window_id, resize_msg->width, resize_msg->height, resize_msg->shm_id);
                    break;
                }
                case COMPOSITOR_MSG_SET_WINDOW_STATE: {
                    compositor_set_window_state_msg_t* state_msg = (compositor_set_window_state_msg_t*)msg.inline_data;
                    compositor_set_window_state(ctx, state_msg->window_id, (window_state_t)state_msg->state);
                    break;
                }
                case COMPOSITOR_MSG_SET_WINDOW_TITLE: {
                    compositor_set_window_title_msg_t* title_msg = (compositor_set_window_title_msg_t*)msg.inline_data;
                    compositor_set_window_title(ctx, title_msg->window_id, title_msg->title);
                    break;
                }
                case COMPOSITOR_MSG_GET_SCREEN_INFO: {
                    ipc_message_t response = {0};
                    response.type = 2; // IPC_MSG_RESPONSE
                    response.msg_id = msg.msg_id;
                    compositor_screen_info_resp_t* screen_info = (compositor_screen_info_resp_t*)response.inline_data;
                    screen_info->width = ctx->screen_width;
                    screen_info->height = ctx->screen_height;
                    response.inline_size = sizeof(compositor_screen_info_resp_t);
                    syscall_raw(SYS_IPC_SEND, msg.sender_tid, (uint64_t)&response, 0, 0, 0);
                    break;
                }
                // Add more message handlers as needed
                default:
                    // Handle mouse/keyboard events which might be sent to clients too
                    if (msg.msg_id == 100) { // MOUSE_BUTTON_EVENT
                        // Route to appropriate window's client
                        // For now, only handle input to focused window if any
                        if (ctx->state->focused_window != 0) {
                            for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
                                if (ctx->state->windows[i].id == ctx->state->focused_window) {
                                    syscall_raw(SYS_IPC_SEND, ctx->state->windows[i].client_ipc_port, (uint64_t)&msg, 0, 0, 0);
                                    break;
                                }
                            }
                        }
                    } else if (msg.msg_id == 101) { // KEYBOARD_EVENT
                         if (ctx->state->focused_window != 0) {
                            for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
                                if (ctx->state->windows[i].id == ctx->state->focused_window) {
                                    syscall_raw(SYS_IPC_SEND, ctx->state->windows[i].client_ipc_port, (uint64_t)&msg, 0, 0, 0);
                                    break;
                                }
                            }
                        }
                    }
                    break;
            }
        }

        // Render if any windows are dirty or if input events occurred
        bool needs_render = false;
        if (ret == 0) needs_render = true; // New message might mean change
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
                mouse_event.sender_tid = syscall_raw(SYS_GETPID,0,0,0,0,0);
                *(uint32_t*)&mouse_event.inline_data[0] = button;
                mouse_event.inline_data[4] = pressed ? 1 : 0;
                *(int32_t*)&mouse_event.inline_data[5] = ctx->mouse_x - win->x; // x coord
                *(int32_t*)&mouse_event.inline_data[9] = ctx->mouse_y - win->y; // y coord
                mouse_event.inline_size = 13;
                
                // Get application's IPC port
                uint64_t app_port = win->client_ipc_port; 
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
                key_event.sender_tid = syscall_raw(SYS_GETPID,0,0,0,0,0);
                *(uint32_t*)&key_event.inline_data[0] = keycode;
                key_event.inline_data[4] = pressed ? 1 : 0;
                key_event.inline_size = 5;
                
                // Get application's IPC port
                uint64_t app_port = win->client_ipc_port; 
                syscall_raw(SYS_IPC_SEND, app_port, (uint64_t)&key_event, 0, 0, 0);
                break;
            }
        }
    }
}