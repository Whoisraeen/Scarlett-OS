/**
 * @file theme.h
 * @brief Theme engine interface
 */

#ifndef THEME_H
#define THEME_H

#include <cstdint>

namespace gui {

class Theme {
public:
    Theme();
    
    uint32_t background_color;
    uint32_t text_color;
    uint32_t button_color;
    uint32_t button_hover_color;
    uint32_t button_pressed_color;
};

} // namespace gui

#endif // THEME_H

