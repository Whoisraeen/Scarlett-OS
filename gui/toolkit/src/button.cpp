/**
 * @file button.cpp
 * @brief Button widget implementation
 */

#include "button.hpp"
#include <cstring>

Button::Button(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const char* label)
    : Widget(x, y, width, height)
    , label_(label ? label : "")
    , pressed_(false)
{
}

Button::~Button() {
}

void Button::render(uint32_t* framebuffer, uint32_t fb_width, uint32_t fb_height) {
    if (!visible_ || !framebuffer) {
        return;
    }
    
    // Simple button rendering
    uint32_t color = pressed_ ? 0xFF808080 : 0xFFC0C0C0;  // Gray when pressed, light gray otherwise
    
    for (uint32_t y = 0; y < height_ && (y_ + y) < fb_height; ++y) {
        for (uint32_t x = 0; x < width_ && (x_ + x) < fb_width; ++x) {
            uint32_t index = (y_ + y) * fb_width + (x_ + x);
            framebuffer[index] = color;
        }
    }
    
    // TODO: Render label text
}

void Button::handle_event(uint32_t event_type, void* event_data) {
    (void)event_data;
    
    if (event_type == EVENT_MOUSE_DOWN) {
        pressed_ = true;
    } else if (event_type == EVENT_MOUSE_UP) {
        if (pressed_ && on_click_) {
            on_click_();
        }
        pressed_ = false;
    }
}

void Button::set_on_click(std::function<void()> callback) {
    on_click_ = callback;
}

