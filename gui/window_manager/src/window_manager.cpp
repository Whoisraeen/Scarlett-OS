/**
 * @file window_manager.cpp
 * @brief Window Manager implementation
 */

#include "window_manager.hpp"
#include <algorithm>

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
    // TODO: Look up compositor service port via IPC
    // compositor_port_ = lookup_service("compositor");
    return true;
}

void WindowManager::run() {
    while (true) {
        // TODO: Receive IPC messages
        // - Window creation requests
        // - Window close requests
        // - Input events
        // - Focus change requests
        
        // TODO: Process pending window operations
        
        // TODO: Send window updates to compositor
        for (const auto& window : windows_) {
            if (window && window->needs_update()) {
                notify_compositor(window.get());
                window->clear_update_flag();
            }
        }
        
        // TODO: Sleep until next event
    }
}

Window* WindowManager::create_window(uint32_t width, uint32_t height, const char* title) {
    // TODO: Create window object
    // auto window = std::make_unique<Window>(width, height, title);
    // Window* ptr = window.get();
    // windows_.push_back(std::move(window));
    // notify_compositor(ptr);
    // return ptr;
    return nullptr;  // Placeholder
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
    
    // TODO: Notify old focused window it lost focus
    if (focused_window_) {
        // focused_window_->on_focus_lost();
    }
    
    focused_window_ = window;
    
    // TODO: Notify new focused window it gained focus
    if (focused_window_) {
        // focused_window_->on_focus_gained();
    }
}

void WindowManager::handle_input(uint32_t type, uint32_t code, int32_t value) {
    // Route input to focused window
    if (focused_window_) {
        // focused_window_->handle_input(type, code, value);
    }
}

void WindowManager::notify_compositor(Window* window) {
    // TODO: Send IPC message to compositor with window update
    // ipc_message_t msg;
    // msg.type = IPC_MSG_WINDOW_UPDATE;
    // msg.data = window->get_render_data();
    // ipc_send(compositor_port_, &msg);
}

