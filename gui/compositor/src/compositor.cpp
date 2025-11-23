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
    
    // Get framebuffer from kernel via syscall
    // In full implementation, would call syscall to get framebuffer info
    // For now, initialize with default values
    if (!init_framebuffer()) {
        return false;
    }
    
    initialized_ = true;
    return true;
}

bool Compositor::init_framebuffer() {
    // Call kernel syscall to get framebuffer info
    // In full implementation, would use SYS_GFX_GET_FB_INFO or similar
    // For now, use placeholder values
    width_ = 1024;
    height_ = 768;
    
    // Map framebuffer memory via syscall
    // In full implementation, would use SYS_MMAP or SYS_GFX_MAP_FB
    // framebuffer_ = (uint32_t*)syscall_map_framebuffer();
    // For now, allocate temporary buffer
    framebuffer_ = (uint32_t*)malloc(width_ * height_ * sizeof(uint32_t));
    if (!framebuffer_) {
        return false;
    }
    
    return true;
}

void Compositor::run() {
    while (true) {
        // Handle IPC messages from window manager
        // In full implementation, would receive IPC messages and process:
        // - Window creation requests
        // - Window destruction requests
        // - Window position/size updates
        // - Window visibility changes
        
        // Handle window update requests
        // Check if any windows need updating and mark them for compositing
        
        // Composite all windows
        composite();
        
        // Present to display
        swap_buffers();
        
        // Sleep until next frame or event
        // In full implementation, would use SYS_SLEEP or wait for vsync
        // For now, yield CPU
        asm volatile("syscall" : : "a"(6) : "rcx", "r11"); // SYS_YIELD
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
            // Render window content to framebuffer
            // In full implementation, would call window->render_to() or similar
            // For now, just mark as rendered
            // Full implementation would:
            // 1. Get window's framebuffer/texture
            // 2. Blit it to compositor framebuffer at window position
            // 3. Apply window decorations if needed
        }
    }
}

void Compositor::swap_buffers() {
    // Call kernel syscall to swap buffers
    // In full implementation, would use SYS_GFX_SWAP_BUFFERS (25)
    asm volatile(
        "syscall"
        :
        : "a"(25)  // SYS_GFX_SWAP_BUFFERS
        : "rcx", "r11", "memory"
    );
}

void Compositor::clear_framebuffer(uint32_t color) {
    if (!framebuffer_) {
        return;
    }
    
    for (uint32_t i = 0; i < width_ * height_; ++i) {
        framebuffer_[i] = color;
    }
}

