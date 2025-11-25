/**
 * @file volume_control.c
 * @brief Volume Control Widget Implementation
 */

#include "volume_control.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../libs/libc/include/syscall.h" // For IPC syscalls
#include "../../libs/libgui/src/graphics.h" // For graphics primitives

// IPC Message IDs for Audio Server (arbitrary for now)
#define AUDIO_SERVER_PORT_NAME "audio_server" // Name if using a name service
#define AUDIO_MSG_SET_VOLUME 1
#define AUDIO_MSG_SET_MUTE 2

// IPC Message structure (must match kernel/include/ipc/ipc.h)
typedef struct {
    uint64_t sender_tid;
    uint64_t msg_id;
    uint32_t type;
    uint32_t inline_size;
    uint8_t inline_data[64];
    void* buffer;
    size_t buffer_size;
} ipc_message_t;

// Syscall wrappers
static uint64_t sys_ipc_create_port(void) {
    return syscall(SYS_IPC_CREATE_PORT, 0, 0, 0, 0, 0);
}

static void sys_set_process_ipc_port(uint64_t port) {
    syscall(SYS_SET_PROCESS_IPC_PORT, port, 0, 0, 0, 0);
}

static int sys_ipc_receive(uint64_t port, ipc_message_t* msg) {
    return (int)syscall(SYS_IPC_RECEIVE, port, (uint64_t)msg, 0, 0, 0);
}

static int sys_ipc_send(uint64_t port, ipc_message_t* msg) {
    return (int)syscall(SYS_IPC_SEND, port, (uint64_t)msg, 0, 0, 0);
}

static long sys_open(const char* path, int flags) {
    return (long)syscall(SYS_OPEN, (uint64_t)path, (uint64_t)flags, 0, 0, 0);
}

static int sys_close(int fd) {
    return (int)syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
}

static long sys_read(int fd, void* buf, size_t count) {
    return (long)syscall(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count, 0, 0);
}

// Global audio server port
static uint64_t g_audio_server_port = 0;
static uint64_t g_my_port = 0;

// Connect to audio server (mock implementation)
static uint64_t audio_client_connect(void) {
    if (g_audio_server_port != 0) return g_audio_server_port;

    // Read audio server port from file (assuming it's published there)
    int fd = sys_open("/var/run/audio_server.port", 0); // O_RDONLY
    if (fd >= 0) {
        sys_read(fd, &g_audio_server_port, sizeof(uint64_t));
        sys_close(fd);
    }
    
    if (g_audio_server_port == 0) {
        printf("VolumeControl: Audio server not found at /var/run/audio_server.port\n");
        return 0;
    }
    g_my_port = syscall(SYS_GET_PROCESS_IPC_PORT, 0, 0, 0, 0, 0);
    return g_audio_server_port;
}

static void audio_client_set_volume(uint32_t volume) {
    uint64_t port = audio_client_connect();
    if (port == 0) return;

    ipc_message_t msg = {0};
    msg.sender_tid = g_my_port;
    msg.msg_id = AUDIO_MSG_SET_VOLUME;
    msg.type = 1; // Request
    *(uint32_t*)&msg.inline_data[0] = volume;
    msg.inline_size = 4;
    sys_ipc_send(port, &msg);
}

static void audio_client_set_mute(bool mute) {
    uint64_t port = audio_client_connect();
    if (port == 0) return;

    ipc_message_t msg = {0};
    msg.sender_tid = g_my_port;
    msg.msg_id = AUDIO_MSG_SET_MUTE;
    msg.type = 1; // Request
    msg.inline_data[0] = mute;
    msg.inline_size = 1;
    sys_ipc_send(port, &msg);
}


volume_control_t* volume_control_create(uint32_t x, uint32_t y) {
    volume_control_t* ctrl = (volume_control_t*)calloc(1, sizeof(volume_control_t));
    if (!ctrl) return NULL;
    
    ctrl->x = x;
    ctrl->y = y;
    ctrl->width = 32;
    ctrl->height = 32;
    
    ctrl->volume = 75;
    ctrl->muted = false;
    ctrl->popup_visible = false;
    
    ctrl->popup_width = 40;
    ctrl->popup_height = 150;
    ctrl->popup_x = x - (ctrl->popup_width / 2) + (ctrl->width / 2);
    ctrl->popup_y = y - ctrl->popup_height - 10;
    
    ctrl->dragging = false;
    
    // Connect to audio server
    audio_client_connect();
    
    return ctrl;
}

void volume_control_destroy(volume_control_t* ctrl) {
    if (ctrl) {
        // No explicit disconnect for now, just free memory
        free(ctrl);
    }
}

void volume_control_handle_click(volume_control_t* ctrl, int32_t x, int32_t y) {
    // Check if click is on icon
    if (x >= (int32_t)ctrl->x && x < (int32_t)(ctrl->x + ctrl->width) &&
        y >= (int32_t)ctrl->y && y < (int32_t)(ctrl->y + ctrl->height)) {
        
        // Toggle popup
        ctrl->popup_visible = !ctrl->popup_visible;
        return;
    }
    
    // Check if click is on popup slider
    if (ctrl->popup_visible &&
        x >= (int32_t)ctrl->popup_x && x < (int32_t)(ctrl->popup_x + ctrl->popup_width) &&
        y >= (int32_t)ctrl->popup_y && y < (int32_t)(ctrl->popup_y + ctrl->popup_height)) {
        
        // Calculate volume from click position
        int32_t rel_y = y - ctrl->popup_y;
        uint32_t new_volume = 100 - ((rel_y * 100) / ctrl->popup_height);
        if (new_volume > 100) new_volume = 100;
        if (new_volume < 0) new_volume = 0;
        
        volume_control_set_volume(ctrl, new_volume);
        ctrl->dragging = true;
        return;
    }
    
    // If click outside, hide popup
    if (ctrl->popup_visible) {
        ctrl->popup_visible = false;
    }
}

void volume_control_handle_mouse_move(volume_control_t* ctrl, int32_t x, int32_t y) {
    if (ctrl->dragging && ctrl->popup_visible) {
        // Update volume based on mouse position
        int32_t rel_y = y - ctrl->popup_y;
        uint32_t new_volume = 100 - ((rel_y * 100) / ctrl->popup_height);
        if (new_volume > 100) new_volume = 100;
        if (new_volume < 0) new_volume = 0;
        
        volume_control_set_volume(ctrl, new_volume);
    }
}

void volume_control_handle_mouse_up(volume_control_t* ctrl) {
    ctrl->dragging = false;
}

void volume_control_handle_scroll(volume_control_t* ctrl, int32_t delta) {
    int32_t new_volume = (int32_t)ctrl->volume + (delta * 5);
    if (new_volume < 0) new_volume = 0;
    if (new_volume > 100) new_volume = 100;
    
    volume_control_set_volume(ctrl, (uint32_t)new_volume);
}

void volume_control_set_volume(volume_control_t* ctrl, uint32_t volume) {
    if (volume > 100) volume = 100;
    
    ctrl->volume = volume;
    audio_client_set_volume(volume);
    
    // Unmute if volume changed from 0
    if (volume > 0 && ctrl->muted) {
        ctrl->muted = false;
        audio_client_set_mute(false);
    }
}

uint32_t volume_control_get_volume(volume_control_t* ctrl) {
    return ctrl->volume;
}

void volume_control_toggle_mute(volume_control_t* ctrl) {
    ctrl->muted = !ctrl->muted;
    audio_client_set_mute(ctrl->muted);
}

void volume_control_render(volume_control_t* ctrl, void* framebuffer_ptr, uint32_t fb_width, uint32_t fb_height) {
    if (!ctrl || !framebuffer_ptr) return;

    gui::GraphicsContext gc((uint32_t*)framebuffer_ptr, fb_width, fb_height);

    // Render volume icon (simple speaker)
    uint32_t icon_color = ctrl->muted ? 0xFFFF0000 : 0xFFFFFFFF; // Red if muted
    
    // Speaker body
    gc.draw_rect(ctrl->x, ctrl->y + 8, 8, 16, icon_color);
    gc.draw_line(ctrl->x + 8, ctrl->y + 8, ctrl->x + 16, ctrl->y, icon_color);
    gc.draw_line(ctrl->x + 8, ctrl->y + 24, ctrl->x + 16, ctrl->y + 32, icon_color);
    gc.draw_line(ctrl->x + 16, ctrl->y, ctrl->x + 16, ctrl->y + 32, icon_color);

    // Mute symbol
    if (ctrl->muted) {
        gc.draw_line(ctrl->x + 5, ctrl->y + 5, ctrl->x + 27, ctrl->y + 27, 0xFFFF0000);
        gc.draw_line(ctrl->x + 5, ctrl->y + 27, ctrl->x + 27, ctrl->y + 5, 0xFFFF0000);
    } else {
        // Volume waves
        int32_t wave_start_x = ctrl->x + 18;
        int32_t wave_y = ctrl->y + 16;
        if (ctrl->volume > 0) gc.draw_line(wave_start_x, wave_y - 4, wave_start_x + 5, wave_y - 8, icon_color);
        if (ctrl->volume > 25) gc.draw_line(wave_start_x + 5, wave_y - 8, wave_start_x + 10, wave_y - 4, icon_color);
        if (ctrl->volume > 50) gc.draw_line(wave_start_x + 10, wave_y - 4, wave_start_x + 15, wave_y - 8, icon_color);
        if (ctrl->volume > 75) gc.draw_line(wave_start_x + 15, wave_y - 8, wave_start_x + 20, wave_y - 4, icon_color);
    }
    
    if (ctrl->popup_visible) {
        volume_control_render_popup(ctrl, framebuffer_ptr, fb_width, fb_height);
    }
}

void volume_control_render_popup(volume_control_t* ctrl, void* framebuffer_ptr, uint32_t fb_width, uint32_t fb_height) {
    if (!ctrl || !framebuffer_ptr) return;

    gui::GraphicsContext gc((uint32_t*)framebuffer_ptr, fb_width, fb_height);

    // Draw background panel
    gc.draw_rect(ctrl->popup_x, ctrl->popup_y, ctrl->popup_width, ctrl->popup_height, 0xFF303030); // Dark gray

    // Draw slider track
    int32_t track_x = ctrl->popup_x + (ctrl->popup_width / 2) - 2;
    int32_t track_y_start = ctrl->popup_y + 10;
    int32_t track_height = ctrl->popup_height - 20;
    gc.draw_rect(track_x, track_y_start, 4, track_height, 0xFF606060); // Gray track

    // Draw slider thumb
    int32_t thumb_height = 10;
    int32_t thumb_y_pos = track_y_start + track_height - ((ctrl->volume * track_height) / 100) - (thumb_height / 2);
    gc.draw_rect(ctrl->popup_x + 5, thumb_y_pos, ctrl->popup_width - 10, thumb_height, 0xFF00BFFF); // Blue thumb

    // Draw volume percentage text
    char vol_text[16];
    snprintf(vol_text, sizeof(vol_text), "%u%%", ctrl->volume);
    gc.draw_text(ctrl->popup_x + 5, ctrl->popup_y + ctrl->popup_height - 18, vol_text, 0xFFFFFFFF); // White text
}