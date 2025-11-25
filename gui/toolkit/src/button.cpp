/**
 * @file button.cpp
 * @brief Button widget implementation
 */

#include "button.hpp"
#include "../../libs/libgui/src/graphics.h" // For GraphicsContext
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
    
    for (uint32_t y_offset = 0; y_offset < height_ && (y_ + y_offset) < fb_height; ++y_offset) {
        for (uint32_t x_offset = 0; x_offset < width_ && (x_ + x_offset) < fb_width; ++x_offset) {
            uint32_t index = (y_ + y_offset) * fb_width + (x_ + x_offset);
            framebuffer[index] = color;
        }
    }
    
    // Render label text
    gui::GraphicsContext gc(framebuffer, fb_width, fb_height);
    // Center the text
    int text_x = x_ + (width_ - label_.length() * 8) / 2; // Assuming 8-pixel font width
    int text_y = y_ + (height_ - 8) / 2; // Assuming 8-pixel font height
    gc.draw_text(text_x, text_y, label_.c_str(), 0xFF000000); // Black text
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

