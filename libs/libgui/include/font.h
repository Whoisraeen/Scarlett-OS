/**
 * @file font.h
 * @brief Font rendering interface
 */

#ifndef FONT_H
#define FONT_H

#include <cstdint>

namespace gui {

class Font {
public:
    void render_char(uint32_t* framebuffer, uint32_t width, int x, int y, char c, uint32_t color);
};

} // namespace gui

#endif // FONT_H

