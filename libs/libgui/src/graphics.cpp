/**
 * @file graphics.cpp
 * @brief Graphics library implementation
 */

#include "graphics.h"
#include "font.h" // Include Font class
#include <cstring>

namespace gui {

GraphicsContext::GraphicsContext(uint32_t* framebuffer, uint32_t width, uint32_t height)
    : framebuffer_(framebuffer)
    , width_(width)
    , height_(height)
{
}

void GraphicsContext::draw_rect(int x, int y, uint32_t width, uint32_t height, uint32_t color) {
    for (uint32_t py = 0; py < height; ++py) {
        for (uint32_t px = 0; px < width; ++px) {
            int fx = x + px;
            int fy = y + py;
            if (fx >= 0 && fy >= 0 && fx < (int)width_ && fy < (int)height_) {
                framebuffer_[fy * width_ + fx] = color;
            }
        }
    }
}

void GraphicsContext::draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
    // Simple line drawing (Bresenham's algorithm would be better)
    int dx = x2 - x1;
    int dy = y2 - y1;
    int steps = (dx > dy ? dx : dy);
    if (steps == 0) return;
    
    float x = (float)x1;
    float y = (float)y1;
    
    float x_inc = (float)dx / steps;
    float y_inc = (float)dy / steps;
    
    for (int i = 0; i <= steps; ++i) {
        int px = (int)x;
        int py = (int)y;
        if (px >= 0 && py >= 0 && px < (int)width_ && py < (int)height_) {
            framebuffer_[py * width_ + px] = color;
        }
        x += x_inc;
        y += y_inc;
    }
}

void GraphicsContext::draw_text(int x, int y, const char* text, uint32_t color) {
    if (!text) return;
    Font font; // Use default font instance
    int current_x = x;
    for (size_t i = 0; i < strlen(text); ++i) {
        font.render_char(framebuffer_, width_, current_x, y, text[i], color);
        current_x += 8; // Assuming 8-pixel wide font
    }
}

} // namespace gui
