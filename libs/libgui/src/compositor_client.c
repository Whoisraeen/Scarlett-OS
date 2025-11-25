/**
 * @file compositor_client.c
 * @brief Compositor Client Implementation
 */

#include "../../gui/widgets/src/widgets.h" // For window_t and widget_t
#include "../include/compositor_ipc.h"    // For IPC message structures
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For sys_getpid
#include <fcntl.h>  // For sys_open flags

// Syscalls
#define SYS_OPEN  3
#define SYS_CLOSE 4
#define SYS_READ  2
#define SYS_IPC_SEND 9
#define SYS_IPC_RECEIVE 10
#define SYS_GETPID 13
#define SYS_GET_PROCESS_IPC_PORT 46
#define SYS_SHM_CREATE 40
#define SYS_SHM_MAP 41
#define SYS_SHM_UNMAP 42
#define SYS_SHM_DESTROY 43

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
    ret = 0;
    #endif
    return ret;
}

static int sys_open(const char* path, int flags) {
    return (int)syscall_raw(SYS_OPEN, (uint64_t)path, (uint64_t)flags, 0, 0, 0);
}

static int sys_close(int fd) {
    return (int)syscall_raw(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
}

static long sys_read(int fd, void* buf, size_t count) {
    return (long)syscall_raw(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count, 0, 0);
}


typedef struct {
    uint64_t sender_tid;
    uint64_t msg_id;
    uint32_t type;
    uint32_t inline_size;
    uint8_t inline_data[64];
    void* buffer; // Not used for this IPC, shared memory for framebuffer
    size_t buffer_size;
} ipc_message_t;


static uint64_t g_compositor_port = 0;
static uint64_t g_my_port = 0; // Client's own IPC port for replies

// Connect to compositor
uint64_t compositor_connect(void) {
    return compositor_connect_internal();
}

// Disconnect from compositor (cleanup)
void compositor_disconnect(void) {
    g_compositor_port = 0;
    g_my_port = 0;
}

// Global function to allow desktop to get screen info
void compositor_get_screen_info(uint32_t* width, uint32_t* height) {
    uint64_t port = compositor_connect_internal();
    if (port == 0) {
        *width = 0; *height = 0;
        return;
    }

    ipc_message_t msg = {0};
    msg.sender_tid = g_my_port;
    msg.msg_id = COMPOSITOR_MSG_GET_SCREEN_INFO;
    msg.type = 1; // Request
    msg.inline_size = 0;

    syscall_raw(SYS_IPC_SEND, port, (uint64_t)&msg, 0, 0, 0);

    ipc_message_t resp;
    syscall_raw(SYS_IPC_RECEIVE, g_my_port, (uint64_t)&resp, 0, 0, 0);

    if (resp.msg_id == COMPOSITOR_MSG_GET_SCREEN_INFO && resp.inline_size == sizeof(compositor_screen_info_resp_t)) {
        compositor_screen_info_resp_t* screen_info = (compositor_screen_info_resp_t*)resp.inline_data;
        *width = screen_info->width;
        *height = screen_info->height;
    } else {
        *width = 0; *height = 0; // Error or unexpected response
    }
}


// Create window
window_t* window_create(const char* title, uint32_t width, uint32_t height) {
    uint64_t port = compositor_connect_internal();
    if (port == 0) return NULL;

    // Allocate shared memory for framebuffer
    uint32_t fb_size = width * height * 4; // RGBA32
    uint64_t shm_id_raw = syscall_raw(SYS_SHM_CREATE, (uint64_t)fb_size, 0, 0, 0, 0);
    if (shm_id_raw == 0) {
        fprintf(stderr, "compositor_client: Failed to create SHM for framebuffer\n");
        return NULL;
    }
    uint32_t shm_id = (uint32_t)shm_id_raw;

    // Map shared memory to our address space
    void* fb_ptr = (void*)syscall_raw(SYS_SHM_MAP, (uint64_t)shm_id, 0, 0, 0, 0);
    if (fb_ptr == NULL) {
        fprintf(stderr, "compositor_client: Failed to map SHM for framebuffer\n");
        syscall_raw(SYS_SHM_DESTROY, (uint64_t)shm_id, 0, 0, 0, 0);
        return NULL;
    }
    
    // Clear the framebuffer
    memset(fb_ptr, 0, fb_size);

    // Prepare IPC message
    ipc_message_t msg = {0};
    msg.sender_tid = g_my_port;
    msg.msg_id = COMPOSITOR_MSG_CREATE_WINDOW;
    msg.type = 1; // Request
    
    compositor_create_window_msg_t create_msg;
    create_msg.pid = (uint32_t)syscall_raw(SYS_GETPID, 0, 0, 0, 0, 0);
    create_msg.x = 100; // Default position
    create_msg.y = 100;
    create_msg.width = width;
    create_msg.height = height;
    create_msg.shm_id = shm_id;
    strncpy(create_msg.title, title ? title : "Window", sizeof(create_msg.title) - 1);
    create_msg.title[sizeof(create_msg.title) - 1] = '\0';

    memcpy(msg.inline_data, &create_msg, sizeof(compositor_create_window_msg_t));
    msg.inline_size = sizeof(compositor_create_window_msg_t);

    syscall_raw(SYS_IPC_SEND, port, (uint64_t)&msg, 0, 0, 0);

    // Wait for response
    ipc_message_t resp;
    syscall_raw(SYS_IPC_RECEIVE, g_my_port, (uint64_t)&resp, 0, 0, 0);
    
    if (resp.msg_id == COMPOSITOR_MSG_CREATE_WINDOW && resp.inline_size >= 4) {
        uint32_t win_id = *(uint32_t*)&resp.inline_data[0];
        if (win_id == 0) { // Compositor failed to create window
            fprintf(stderr, "compositor_client: Compositor failed to create window\n");
            syscall_raw(SYS_SHM_UNMAP, (uint64_t)shm_id, 0, 0, 0, 0);
            syscall_raw(SYS_SHM_DESTROY, (uint64_t)shm_id, 0, 0, 0, 0);
            return NULL;
        }

        window_t* win = (window_t*)malloc(sizeof(window_t));
        if (!win) {
            fprintf(stderr, "compositor_client: Failed to allocate window_t\n");
            syscall_raw(SYS_SHM_UNMAP, (uint64_t)shm_id, 0, 0, 0, 0);
            syscall_raw(SYS_SHM_DESTROY, (uint64_t)shm_id, 0, 0, 0, 0);
            return NULL;
        }
        
        memset(win, 0, sizeof(window_t));
        win->compositor_id = win_id;
        win->width = width;
        win->height = height;
        win->visible = true;
        win->title = strdup(title ? title : "Window");
        win->framebuffer = fb_ptr; // Our mapped shared memory
        win->shm_id = shm_id;
        win->framebuffer_size = fb_size;
        
        fprintf(stderr, "compositor_client: Window created: ID %u, SHM %u, FB %p\n", win_id, shm_id, fb_ptr);
        return win;
    }

    fprintf(stderr, "compositor_client: Unexpected response for create window\n");
    syscall_raw(SYS_SHM_UNMAP, (uint64_t)shm_id, 0, 0, 0, 0);
    syscall_raw(SYS_SHM_DESTROY, (uint64_t)shm_id, 0, 0, 0, 0);
    return NULL;
}

void window_destroy(window_t* window) {
    if (!window) return; 
    
    uint64_t port = compositor_connect_internal();
    if (port != 0) {
        ipc_message_t msg = {0};
        msg.sender_tid = g_my_port;
        msg.msg_id = COMPOSITOR_MSG_DESTROY_WINDOW;
        msg.type = 1;
        
        compositor_destroy_window_msg_t destroy_msg;
        destroy_msg.window_id = window->compositor_id;
        memcpy(msg.inline_data, &destroy_msg, sizeof(compositor_destroy_window_msg_t));
        msg.inline_size = sizeof(compositor_destroy_window_msg_t);
        
        syscall_raw(SYS_IPC_SEND, port, (uint64_t)&msg, 0, 0, 0);
        // No response needed for destroy
    }
    
    // Unmap and destroy shared memory
    if (window->framebuffer && window->shm_id != 0) {
        syscall_raw(SYS_SHM_UNMAP, (uint64_t)window->shm_id, 0, 0, 0, 0);
        syscall_raw(SYS_SHM_DESTROY, (uint64_t)window->shm_id, 0, 0, 0, 0);
    }

    if (window->title) free(window->title);
    if (window->framebuffer) free(window->framebuffer); // This was just a pointer to SHM
    free(window);
}

void window_show(window_t* window) {
    if (!window) return;

    uint64_t port = compositor_connect_internal();
    if (port == 0) return;

    ipc_message_t msg = {0};
    msg.sender_tid = g_my_port;
    msg.msg_id = COMPOSITOR_MSG_SET_WINDOW_STATE;
    msg.type = 1;
    
    compositor_set_window_state_msg_t state_msg;
    state_msg.window_id = window->compositor_id;
    state_msg.state = COMPOSITOR_WINDOW_STATE_NORMAL;
    memcpy(msg.inline_data, &state_msg, sizeof(compositor_set_window_state_msg_t));
    msg.inline_size = sizeof(compositor_set_window_state_msg_t);

    syscall_raw(SYS_IPC_SEND, port, (uint64_t)&msg, 0, 0, 0);
    window->visible = true;
}

void window_hide(window_t* window) {
    if (!window) return;

    uint64_t port = compositor_connect_internal();
    if (port == 0) return;

    ipc_message_t msg = {0};
    msg.sender_tid = g_my_port;
    msg.msg_id = COMPOSITOR_MSG_SET_WINDOW_STATE;
    msg.type = 1;
    
    compositor_set_window_state_msg_t state_msg;
    state_msg.window_id = window->compositor_id;
    state_msg.state = COMPOSITOR_WINDOW_STATE_HIDDEN;
    memcpy(msg.inline_data, &state_msg, sizeof(compositor_set_window_state_msg_t));
    msg.inline_size = sizeof(compositor_set_window_state_msg_t);

    syscall_raw(SYS_IPC_SEND, port, (uint64_t)&msg, 0, 0, 0);
    window->visible = false;
}

void window_set_title(window_t* window, const char* title) {
    if (!window) return;

    uint64_t port = compositor_connect_internal();
    if (port == 0) return;

    ipc_message_t msg = {0};
    msg.sender_tid = g_my_port;
    msg.msg_id = COMPOSITOR_MSG_SET_WINDOW_TITLE;
    msg.type = 1;
    
    compositor_set_window_title_msg_t title_msg;
    title_msg.window_id = window->compositor_id;
    strncpy(title_msg.title, title, sizeof(title_msg.title) - 1);
    title_msg.title[sizeof(title_msg.title) - 1] = '\0';
    memcpy(msg.inline_data, &title_msg, sizeof(compositor_set_window_title_msg_t));
    msg.inline_size = sizeof(compositor_set_window_title_msg_t);

    syscall_raw(SYS_IPC_SEND, port, (uint64_t)&msg, 0, 0, 0);
    if (window->title) free(window->title);
    window->title = strdup(title);
}

void window_add_widget(window_t* window, widget_t* widget) {
    // Widgets are drawn directly to the window's framebuffer. No IPC needed.
    // This function will probably be handled by the widget toolkit.
}

void window_render(window_t* window) {
    if (!window) return;
    // Request compositor to redraw this window.
    uint64_t port = compositor_connect_internal();
    if (port == 0) return;

    ipc_message_t msg = {0};
    msg.sender_tid = g_my_port;
    msg.msg_id = COMPOSITOR_MSG_RENDER_WINDOW;
    msg.type = 1;
    *(uint32_t*)&msg.inline_data[0] = window->compositor_id;
    msg.inline_size = 4;
    
    syscall_raw(SYS_IPC_SEND, port, (uint64_t)&msg, 0, 0, 0);
}