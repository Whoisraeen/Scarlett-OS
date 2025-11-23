/**
 * @file window_manager.cpp
 * @brief Window Manager implementation
 */

#include "window_manager.hpp"
#include <algorithm>
#include <cstring>

// IPC message structure (matches kernel/include/ipc/ipc.h)
struct ipc_message {
    uint64_t sender_tid;
    uint64_t msg_id;
    uint32_t type;
    uint32_t inline_size;
    uint8_t inline_data[64];
    void* buffer;
    size_t buffer_size;
};

// IPC message types
#define IPC_MSG_REQUEST 1
#define IPC_MSG_RESPONSE 2

// Compositor message IDs
#define COMPOSITOR_MSG_CREATE_WINDOW 1
#define COMPOSITOR_MSG_DESTROY_WINDOW 2
#define COMPOSITOR_MSG_MOVE_WINDOW 3
#define COMPOSITOR_MSG_RESIZE_WINDOW 4

// Syscall numbers
#define SYS_IPC_SEND 9
#define SYS_IPC_RECEIVE 10
#define SYS_IPC_CREATE_PORT 26
#define SYS_YIELD 6

// Raw syscall wrapper (x86_64)
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

WindowManager::WindowManager()
    : focused_window_(nullptr)
    , compositor_port_(0)
    , initialized_(false)
{
}

WindowManager::~WindowManager() {
    // Windows will be automatically destroyed via unique_ptr
}

bool WindowManager::init() {
    if (initialized_) {
        return true;
    }
    
    // Connect to compositor service
    if (!connect_to_compositor()) {
        return false;
    }
    
    initialized_ = true;
    return true;
}

bool WindowManager::connect_to_compositor() {
    // Look up compositor service port via IPC
    // In a full implementation, would use service discovery mechanism
    // For now, use well-known port (compositor registers this during init)
    compositor_port_ = 100; // Well-known compositor port
    return true;
}

void WindowManager::run() {
    while (true) {
        // Receive IPC messages from applications
        ipc_message msg;
        int ret = (int)syscall_raw(SYS_IPC_RECEIVE, compositor_port_, (uint64_t)&msg, 0, 0, 0);
        if (ret == 0) {
            // Handle message based on msg_id
            switch (msg.msg_id) {
                case COMPOSITOR_MSG_CREATE_WINDOW:
                    // Application requested window creation
                    // This would be handled by the compositor, not the window manager
                    break;
                case COMPOSITOR_MSG_DESTROY_WINDOW:
                    // Application requested window destruction
                    if (msg.inline_size >= 4) {
                        uint32_t win_id = *(uint32_t*)&msg.inline_data[0];
                        // Find and destroy window
                        for (auto it = windows_.begin(); it != windows_.end(); ++it) {
                            if ((*it)->id() == win_id) {
                                destroy_window(it->get());
                                break;
                            }
                        }
                    }
                    break;
                // Add more message handlers as needed
            }
        }
        
        // Process pending window operations
        // Check for windows that need position/size updates, state changes, etc.
        for (const auto& window : windows_) {
            if (window && window->needs_update()) {
                notify_compositor(window.get());
                window->clear_update_flag();
            }
        }
        
        // Yield CPU
        syscall_raw(SYS_YIELD, 0, 0, 0, 0, 0);
    }
}

Window* WindowManager::create_window(uint32_t width, uint32_t height, const char* title) {
    // Send IPC message to compositor to create window
    ipc_message msg;
    msg.type = IPC_MSG_REQUEST;
    msg.msg_id = COMPOSITOR_MSG_CREATE_WINDOW;
    msg.sender_tid = 0; // Will be filled by kernel
    
    // Pack window creation parameters into inline data
    uint32_t* data = (uint32_t*)msg.inline_data;
    data[0] = width;
    data[1] = height;
    // Copy title (max 56 chars to fit in remaining 56 bytes)
    size_t title_len = strlen(title);
    if (title_len > 56) title_len = 56;
    memcpy(&data[2], title, title_len);
    msg.inline_size = 8 + (uint32_t)title_len;
    
    // Send message to compositor
    int ret = (int)syscall_raw(SYS_IPC_SEND, compositor_port_, (uint64_t)&msg, 0, 0, 0);
    if (ret != 0) {
        return nullptr;
    }
    
    // Wait for response with window ID
    ipc_message response;
    ret = (int)syscall_raw(SYS_IPC_RECEIVE, compositor_port_, (uint64_t)&response, 0, 0, 0);
    if (ret != 0 || response.type != IPC_MSG_RESPONSE) {
        return nullptr;
    }
    
    uint32_t win_id = *(uint32_t*)&response.inline_data[0];
    
    // Create window object (would use actual Window class constructor)
    // For now, this is a placeholder - Window class needs to be implemented
    // auto window = std::make_unique<Window>(win_id, width, height, title);
    // Window* ptr = window.get();
    // windows_.push_back(std::move(window));
    // return ptr;
    
    // Placeholder - Window class needs to be implemented
    return nullptr;
}

void WindowManager::destroy_window(Window* window) {
    windows_.erase(
        std::remove_if(windows_.begin(), windows_.end(),
            [window](const std::unique_ptr<Window>& w) {
                return w.get() == window;
            }),
        windows_.end()
    );
    
    if (focused_window_ == window) {
        focused_window_ = nullptr;
    }
}

void WindowManager::set_focus(Window* window) {
    if (focused_window_ == window) {
        return;
    }
    
    // Notify old focused window it lost focus
    if (focused_window_) {
        // In full implementation, would call window's on_focus_lost() method
        // focused_window_->on_focus_lost();
        // Would also send IPC notification to application
    }
    
    focused_window_ = window;
    
    // Notify new focused window it gained focus
    if (focused_window_) {
        // In full implementation, would call window's on_focus_gained() method
        // focused_window_->on_focus_gained();
        // Would also send IPC notification to application
    }
}

void WindowManager::handle_input(uint32_t type, uint32_t code, int32_t value) {
    // Route input to focused window
    if (focused_window_) {
        // focused_window_->handle_input(type, code, value);
    }
}

void WindowManager::notify_compositor(Window* window) {
    if (!window) return;
    
    // Send IPC message to compositor with window update
    ipc_message msg;
    msg.type = IPC_MSG_REQUEST;
    msg.sender_tid = 0; // Will be filled by kernel
    
    // Pack window update data into inline data
    // This would include position, size, state changes, etc.
    uint32_t* data = (uint32_t*)msg.inline_data;
    data[0] = window->id();
    data[1] = window->x();
    data[2] = window->y();
    data[3] = window->width();
    data[4] = window->height();
    msg.inline_size = 20;
    msg.msg_id = COMPOSITOR_MSG_MOVE_WINDOW; // Or appropriate message type
    
    // Send message to compositor
    syscall_raw(SYS_IPC_SEND, compositor_port_, (uint64_t)&msg, 0, 0, 0);
}

