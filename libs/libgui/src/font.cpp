/**
 * @file font.cpp
 * @brief Font rendering implementation
 */

#include "font.h"
#include "font8x8_basic.h" // Assuming font data is alongside

namespace gui {

void Font::render_char(uint32_t* framebuffer, uint32_t width, int x, int y, char c, uint32_t color) {
    if (c < 0 || c > 127) return; // Only ASCII for now
    for (int dy = 0; dy < 8; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            if ((font8x8_basic[(int)c][dy] >> dx) & 1) {
                // Check bounds (simplified, assuming target framebuffer has sufficient height)
                if (x + dx >= 0 && x + dx < (int)width && y + dy >= 0) {
                    framebuffer[(y + dy) * width + (x + dx)] = color;
                }
            }
        }
    }
}

} // namespace gui

