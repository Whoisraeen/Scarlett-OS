/**
 * @file theme.cpp
 * @brief Theme engine implementation
 */

#include "theme.h"

namespace gui {

Theme::Theme() {
    // Default theme colors
    background_color = 0xFFF0F0F0;
    text_color = 0xFF000000;
    button_color = 0xFFE0E0E0;
    button_hover_color = 0xFFD0D0D0;
    button_pressed_color = 0xFFC0C0C0;
}

} // namespace gui

