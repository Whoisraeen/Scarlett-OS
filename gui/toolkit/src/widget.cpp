/**
 * @file widget.cpp
 * @brief Widget implementation
 */

#include "widget.hpp"

Widget::Widget(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    : x_(x)
    , y_(y)
    , width_(width)
    , height_(height)
    , visible_(true)
{
}

Widget::~Widget() {
}

void Widget::render(uint32_t* framebuffer, uint32_t fb_width, uint32_t fb_height) {
    // Base implementation does nothing
    (void)framebuffer;
    (void)fb_width;
    (void)fb_height;
}

void Widget::handle_event(uint32_t event_type, void* event_data) {
    // Base implementation does nothing
    (void)event_type;
    (void)event_data;
}

void Widget::set_position(uint32_t x, uint32_t y) {
    x_ = x;
    y_ = y;
}

void Widget::set_size(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
}

void Widget::set_visible(bool visible) {
    visible_ = visible;
}

