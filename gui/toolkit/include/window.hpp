/**
 * @file window.hpp
 * @brief Window widget
 */

#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "widget.hpp"
#include <vector>
#include <string>

/**
 * Window widget (top-level container)
 */
class Window : public Widget {
public:
    Window(uint32_t width, uint32_t height, const char* title);
    ~Window();
    
    void render(uint32_t* framebuffer, uint32_t fb_width, uint32_t fb_height) override;
    
    void add_widget(Widget* widget);
    void remove_widget(Widget* widget);
    
    bool needs_update() const { return needs_update_; }
    void clear_update_flag() { needs_update_ = false; }
    void set_needs_update() { needs_update_ = true; }

private:
    std::string title_;
    std::vector<Widget*> children_;
    bool needs_update_;
};

#endif // WINDOW_HPP

