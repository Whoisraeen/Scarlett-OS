/**
 * @file graphics.h
 * @brief Graphics library for GUI applications
 */

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <cstdint>

namespace gui {

class GraphicsContext {
public:
    GraphicsContext(uint32_t* framebuffer, uint32_t width, uint32_t height);
    
    void draw_rect(int x, int y, uint32_t width, uint32_t height, uint32_t color);
    void draw_line(int x1, int y1, int x2, int y2, uint32_t color);
    void draw_text(int x, int y, const char* text, uint32_t color);
    
private:
    uint32_t* framebuffer_;
    uint32_t width_;
    uint32_t height_;
};

} // namespace gui

#endif // GRAPHICS_H

