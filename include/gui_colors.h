#ifndef GUI_COLORS_H
#define GUI_COLORS_H

#include <Arduino.h>

// Color scheme matching BioPal WebUI
// Gradient colors (purple-blue to purple)
#define COLOR_PRIMARY_START     0x63FD  // #667eea - Purple-blue
#define COLOR_PRIMARY_END       0x7254  // #764ba2 - Purple

// Status colors
#define COLOR_SUCCESS           0x2D28  // #28a745 - Green
#define COLOR_DANGER            0xD9A8  // #dc3545 - Red

// Neutral colors
#define COLOR_BG_LIGHT          0xFFDF  // #f8f9fa - Very light gray
#define COLOR_BG_MEDIUM         0xE71C  // #e9ecef - Light gray
#define COLOR_TEXT_DARK         0x3186  // #333333 - Dark gray
#define COLOR_TEXT_GRAY         0x7BEF  // #6c757d - Medium gray

// Standard colors
#define COLOR_WHITE             TFT_WHITE
#define COLOR_BLACK             TFT_BLACK
#define COLOR_GRID              TFT_DARKGREY

// Helper macro for creating gradient effect
// Linearly interpolate between two 16-bit colors
inline uint16_t lerpColor(uint16_t color1, uint16_t color2, float t) {
    if (t <= 0.0f) return color1;
    if (t >= 1.0f) return color2;

    // Extract RGB components from RGB565
    uint8_t r1 = (color1 >> 11) & 0x1F;
    uint8_t g1 = (color1 >> 5) & 0x3F;
    uint8_t b1 = color1 & 0x1F;

    uint8_t r2 = (color2 >> 11) & 0x1F;
    uint8_t g2 = (color2 >> 5) & 0x3F;
    uint8_t b2 = color2 & 0x1F;

    // Interpolate
    uint8_t r = r1 + (uint8_t)((r2 - r1) * t);
    uint8_t g = g1 + (uint8_t)((g2 - g1) * t);
    uint8_t b = b1 + (uint8_t)((b2 - b1) * t);

    // Recombine to RGB565
    return (r << 11) | (g << 5) | b;
}

#endif // GUI_COLORS_H
