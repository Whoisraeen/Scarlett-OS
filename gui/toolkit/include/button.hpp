/**
 * @file button.hpp
 * @brief Button widget
 */

#ifndef BUTTON_HPP
#define BUTTON_HPP

#include "widget.hpp"
#include <functional>
#include <string>

/**
 * Button widget
 */
class Button : public Widget {
public:
    Button(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const char* label);
    ~Button();
    
    void render(uint32_t* framebuffer, uint32_t fb_width, uint32_t fb_height) override;
    void handle_event(uint32_t event_type, void* event_data) override;
    
    void set_on_click(std::function<void()> callback);

private:
    std::string label_;
    bool pressed_;
    std::function<void()> on_click_;
    
    enum {
        EVENT_MOUSE_DOWN = 1,
        EVENT_MOUSE_UP = 2,
    };
};

#endif // BUTTON_HPP

