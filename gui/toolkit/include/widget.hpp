/**
 * @file widget.hpp
 * @brief Base widget class for GUI toolkit
 */

#ifndef WIDGET_HPP
#define WIDGET_HPP

#include <cstdint>

/**
 * Base widget class
 */
class Widget {
public:
    Widget(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    virtual ~Widget();
    
    virtual void render(uint32_t* framebuffer, uint32_t fb_width, uint32_t fb_height);
    virtual void handle_event(uint32_t event_type, void* event_data);
    
    void set_position(uint32_t x, uint32_t y);
    void set_size(uint32_t width, uint32_t height);
    void set_visible(bool visible);
    
    uint32_t get_x() const { return x_; }
    uint32_t get_y() const { return y_; }
    uint32_t get_width() const { return width_; }
    uint32_t get_height() const { return height_; }
    bool is_visible() const { return visible_; }

protected:
    uint32_t x_, y_;
    uint32_t width_, height_;
    bool visible_;
};

#endif // WIDGET_HPP

