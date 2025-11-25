#ifndef LIBS_LIBGUI_INCLUDE_COMPOSITOR_IPC_H
#define LIBS_LIBGUI_INCLUDE_COMPOSITOR_IPC_H

#include <stdint.h>

// IPC Message IDs for Compositor Service
#define COMPOSITOR_MSG_CREATE_WINDOW      1
#define COMPOSITOR_MSG_DESTROY_WINDOW     2
#define COMPOSITOR_MSG_MOVE_WINDOW        3
#define COMPOSITOR_MSG_RESIZE_WINDOW      4
#define COMPOSITOR_MSG_SET_WINDOW_STATE   5
#define COMPOSITOR_MSG_SET_WINDOW_TITLE   6
#define COMPOSITOR_MSG_RAISE_WINDOW       7
#define COMPOSITOR_MSG_FOCUS_WINDOW       8
#define COMPOSITOR_MSG_RENDER_WINDOW      9 // Request a redraw for a dirty window
#define COMPOSITOR_MSG_GET_SCREEN_INFO    10

// Window States (matching compositor.h)
typedef enum {
    COMPOSITOR_WINDOW_STATE_HIDDEN = 0,
    COMPOSITOR_WINDOW_STATE_NORMAL = 1,
    COMPOSITOR_WINDOW_STATE_MINIMIZED = 2,
    COMPOSITOR_WINDOW_STATE_MAXIMIZED = 3,
    COMPOSITOR_WINDOW_STATE_FULLSCREEN = 4,
} compositor_window_state_t;

// IPC Message structure (simplified for client-server)
// This structure will be used for serializing arguments for IPC calls
// Note: Actual IPC message structure (ipc_message_t) is in sdk/include/scarlettos/ipc.h

// Data for COMPOSITOR_MSG_CREATE_WINDOW
typedef struct {
    uint32_t pid;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t shm_id; // Shared memory ID for framebuffer
    char title[44]; // Title (max 43 chars + null, to fit inline_data with shm_id)
} compositor_create_window_msg_t;

// Data for COMPOSITOR_MSG_DESTROY_WINDOW
typedef struct {
    uint32_t window_id;
} compositor_destroy_window_msg_t;

// Data for COMPOSITOR_MSG_MOVE_WINDOW
typedef struct {
    uint32_t window_id;
    int32_t x;
    int32_t y;
} compositor_move_window_msg_t;

// Data for COMPOSITOR_MSG_RESIZE_WINDOW
typedef struct {
    uint32_t window_id;
    uint32_t width;
    uint32_t height;
    uint32_t shm_id; // New SHM ID if framebuffer changed
} compositor_resize_window_msg_t;

// Data for COMPOSITOR_MSG_SET_WINDOW_STATE
typedef struct {
    uint32_t window_id;
    compositor_window_state_t state;
} compositor_set_window_state_msg_t;

// Data for COMPOSITOR_MSG_SET_WINDOW_TITLE
typedef struct {
    uint32_t window_id;
    char title[60]; // Max 60 chars to fit in inline_data
} compositor_set_window_title_msg_t;

// Data for COMPOSITOR_MSG_GET_SCREEN_INFO response
typedef struct {
    uint32_t width;
    uint32_t height;
} compositor_screen_info_resp_t;


#endif // LIBS_LIBGUI_INCLUDE_COMPOSITOR_IPC_H