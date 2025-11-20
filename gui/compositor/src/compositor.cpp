/**
 * @file compositor.cpp
 * @brief Compositor implementation
 */

#include "compositor.hpp"
#include <cstring>
#include <cstdlib>

Compositor::Compositor()
    : framebuffer_(nullptr)
    , width_(0)
    , height_(0)
    , initialized_(false)
{
}

Compositor::~Compositor() {
    // Cleanup
}

bool Compositor::init() {
    if (initialized_) {
        return true;
    }
    
    // TODO: Get framebuffer from kernel via syscall
    // For now, initialize with default values
    if (!init_framebuffer()) {
        return false;
    }
    
    initialized_ = true;
    return true;
}

bool Compositor::init_framebuffer() {
    // TODO: Call kernel syscall to get framebuffer info
    // For now, use placeholder values
    width_ = 1024;
    height_ = 768;
    
    // TODO: Map framebuffer memory via syscall
    // framebuffer_ = (uint32_t*)syscall_map_framebuffer();
    
    return true;
}

void Compositor::run() {
    while (true) {
        // TODO: Handle IPC messages from window manager
        // TODO: Handle window update requests
        
        // Composite all windows
        composite();
        
        // Present to display
        swap_buffers();
        
        // TODO: Sleep until next frame or event
    }
}

void Compositor::register_window(Window* window) {
    if (window) {
        windows_.push_back(window);
    }
}

void Compositor::unregister_window(Window* window) {
    for (auto it = windows_.begin(); it != windows_.end(); ++it) {
        if (*it == window) {
            windows_.erase(it);
            break;
        }
    }
}

void Compositor::composite() {
    // Clear framebuffer
    clear_framebuffer(0x00000000);  // Black background
    
    // Composite windows from bottom to top (back to front)
    for (Window* window : windows_) {
        if (window && window->is_visible()) {
            // TODO: Render window content to framebuffer
            // window->render_to(framebuffer_, width_, height_);
        }
    }
}

void Compositor::swap_buffers() {
    // TODO: Call kernel syscall to swap buffers
    // syscall_gfx_swap_buffers();
}

void Compositor::clear_framebuffer(uint32_t color) {
    if (!framebuffer_) {
        return;
    }
    
    for (uint32_t i = 0; i < width_ * height_; ++i) {
        framebuffer_[i] = color;
    }
}

