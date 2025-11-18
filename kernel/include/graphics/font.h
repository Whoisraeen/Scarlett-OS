/**
 * @file font.h
 * @brief Font interface
 */

#ifndef KERNEL_GRAPHICS_FONT_H
#define KERNEL_GRAPHICS_FONT_H

#include "../types.h"

/**
 * Get font glyph data for a character
 * @param c Character to get glyph for
 * @return Pointer to 8-byte glyph data (8 rows, 1 byte per row)
 */
const uint8_t* font_get_glyph(char c);

#endif // KERNEL_GRAPHICS_FONT_H

