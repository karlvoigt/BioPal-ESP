#ifndef GUI_SCREENS_H
#define GUI_SCREENS_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "gui_state.h"
#include "gui_colors.h"

/*=========================SCREEN DIMENSIONS=========================*/

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

/*=========================TFT INSTANCE=========================*/

// Get reference to TFT instance (initialized in bode_plot.cpp or gui_screens.cpp)
extern TFT_eSPI tft;

// Forward declarations of external functions (to avoid header conflicts)
extern bool isBLEConnected();

/*=========================SCREEN RENDERING FUNCTIONS=========================*/

// Render the current screen based on GUI state
// Should be called whenever screen needs updating
void renderCurrentScreen();

// Individual screen rendering functions
void drawSplashScreen();
void drawHomeScreen();
void drawSettingsScreen();
void drawFreqOverrideScreen();
void drawProgressScreen(bool isBaseline);  // Used for both baseline and final
void drawBaselineCompleteScreen();
void drawResultsScreen();

/*=========================HELPER DRAWING FUNCTIONS=========================*/

// Draw a gradient-filled rectangle
void drawGradientRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color1, uint16_t color2, bool horizontal);

// Draw centered text
void drawCenteredText(const char* text, int16_t y, uint8_t font, uint16_t color);

// Draw a rounded rectangle with border
void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t fillColor, uint16_t borderColor);

// Draw a button (rounded rect with text)
void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char* text, bool highlighted, bool large = false);

// Draw progress bar with gradient fill
void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, float percent);

// Draw BLE connection indicator (small dot)
void drawConnectionIndicator(int16_t x, int16_t y, bool connected);

// Draw DUT status grid (for progress screen)
void drawDUTStatusGrid(int16_t x, int16_t y);

// Draw a simple icon (using basic shapes)
void drawCheckmark(int16_t x, int16_t y, int16_t size, uint16_t color);

#endif // GUI_SCREENS_H
