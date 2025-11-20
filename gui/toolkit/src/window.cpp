/**
 * @file window.cpp
 * @brief Window widget implementation
 */

#include "window.hpp"
#include <algorithm>

Window::Window(uint32_t width, uint32_t height, const char* title)
    : Widget(0, 0, width, height)
    , title_(title ? title : "")
    , needs_update_(true)
{
}

Window::~Window() {
}

void Window::render(uint32_t* framebuffer, uint32_t fb_width, uint32_t fb_height) {
    if (!visible_ || !framebuffer) {
        return;
    }
    
    // Render window background
    uint32_t bg_color = 0xFFFFFFFF;  // White
    
    for (uint32_t y = 0; y < height_ && (y_ + y) < fb_height; ++y) {
        for (uint32_t x = 0; x < width_ && (x_ + x) < fb_width; ++x) {
            uint32_t index = (y_ + y) * fb_width + (x_ + x);
            framebuffer[index] = bg_color;
        }
    }
    
    // Render title bar
    uint32_t title_color = 0xFF0000FF;  // Blue
    uint32_t title_height = 30;
    
    for (uint32_t y = 0; y < title_height && y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            uint32_t index = (y_ + y) * fb_width + (x_ + x);
            framebuffer[index] = title_color;
        }
    }
    
    // Render child widgets
    for (Widget* widget : children_) {
        if (widget && widget->is_visible()) {
            widget->render(framebuffer, fb_width, fb_height);
        }
    }
}

void Window::add_widget(Widget* widget) {
    if (widget) {
        children_.push_back(widget);
        needs_update_ = true;
    }
}

void Window::remove_widget(Widget* widget) {
    children_.erase(
        std::remove(children_.begin(), children_.end(), widget),
        children_.end()
    );
    needs_update_ = true;
}

