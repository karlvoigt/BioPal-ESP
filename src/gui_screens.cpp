#include "gui_screens.h"
#include "defines.h"

// TFT instance (shared with bode_plot.cpp)
extern TFT_eSPI tft;

// External state variables (from gui_state.cpp)
extern GUIState currentGUIState;
extern GUISettings guiSettings;
extern uint8_t selectedDUTCount;
extern uint8_t menuSelection;
extern bool menuEditMode;
extern uint8_t currentDUT;
extern uint8_t totalDUTs;
extern float progressPercent;
extern bool dutStatus[4];

/*=========================HELPER DRAWING FUNCTIONS=========================*/

void drawGradientRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color1, uint16_t color2, bool horizontal) {
    if (horizontal) {
        // Horizontal gradient
        for (int16_t i = 0; i < w; i++) {
            float t = (float)i / (float)w;
            uint16_t color = lerpColor(color1, color2, t);
            tft.drawFastVLine(x + i, y, h, color);
        }
    } else {
        // Vertical gradient
        for (int16_t i = 0; i < h; i++) {
            float t = (float)i / (float)h;
            uint16_t color = lerpColor(color1, color2, t);
            tft.drawFastHLine(x, y + i, w, color);
        }
    }
}

void drawCenteredText(const char* text, int16_t y, uint8_t font, uint16_t color) {
    tft.setTextColor(color);
    tft.setTextDatum(TC_DATUM);  // Top center
    tft.drawString(text, SCREEN_WIDTH / 2, y, font);
}

void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t fillColor, uint16_t borderColor) {
    tft.fillRoundRect(x, y, w, h, r, fillColor);
    if (borderColor != fillColor) {
        tft.drawRoundRect(x, y, w, h, r, borderColor);
    }
}

void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char* text, bool highlighted, bool large) {
    uint16_t fillColor, textColor;

    if (highlighted) {
        // Gradient fill for highlighted button
        drawGradientRect(x, y, w, h, COLOR_PRIMARY_START, COLOR_PRIMARY_END, true);
        textColor = COLOR_WHITE;
        tft.drawRoundRect(x, y, w, h, 8, COLOR_PRIMARY_END);
    } else {
        fillColor = COLOR_BG_MEDIUM;
        textColor = COLOR_TEXT_DARK;
        drawRoundRect(x, y, w, h, 8, fillColor, fillColor);
    }

    // Draw text centered in button
    tft.setTextColor(textColor);
    tft.setTextDatum(MC_DATUM);  // Middle center
    tft.drawString(text, x + w/2, y + h/2, large ? 4 : 2);
}

void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, float percent) {
    // Background
    tft.fillRoundRect(x, y, w, h, h/2, COLOR_BG_MEDIUM);

    // Calculate fill width
    int16_t fillWidth = (int16_t)((float)w * percent / 100.0f);
    if (fillWidth > 0) {
        // Draw gradient fill
        drawGradientRect(x, y, fillWidth, h, COLOR_PRIMARY_START, COLOR_PRIMARY_END, true);

        // Draw percentage text in center
        char percentText[8];
        snprintf(percentText, sizeof(percentText), "%.0f%%", percent);
        tft.setTextColor(COLOR_WHITE);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(percentText, x + w/2, y + h/2, 2);
    }
}

void drawConnectionIndicator(int16_t x, int16_t y, bool connected) {
    uint16_t color = connected ? COLOR_SUCCESS : COLOR_DANGER;
    tft.fillCircle(x, y, 5, color);

    // Add subtle glow effect for connected state
    if (connected) {
        tft.drawCircle(x, y, 7, lerpColor(COLOR_SUCCESS, COLOR_BG_LIGHT, 0.5));
    }
}

void drawDUTStatusGrid(int16_t x, int16_t y) {
    const int16_t boxSize = 60;
    const int16_t gap = 10;
    const int16_t cols = 2;

    for (uint8_t i = 0; i < totalDUTs; i++) {
        int16_t col = i % cols;
        int16_t row = i / cols;
        int16_t boxX = x + col * (boxSize + gap);
        int16_t boxY = y + row * (boxSize + gap);

        // Determine status color
        uint16_t fillColor, borderColor;
        if (dutStatus[i]) {
            // Complete
            fillColor = lerpColor(COLOR_SUCCESS, COLOR_WHITE, 0.7);
            borderColor = COLOR_SUCCESS;
        } else if (i == currentDUT && progressPercent > 0) {
            // Currently measuring
            fillColor = lerpColor(COLOR_PRIMARY_START, COLOR_WHITE, 0.8);
            borderColor = COLOR_PRIMARY_START;
        } else {
            // Pending
            fillColor = COLOR_BG_LIGHT;
            borderColor = COLOR_BG_MEDIUM;
        }

        // Draw box
        drawRoundRect(boxX, boxY, boxSize, boxSize, 8, fillColor, borderColor);

        // Draw DUT label
        char label[8];
        snprintf(label, sizeof(label), "DUT %d", i + 1);
        tft.setTextColor(COLOR_TEXT_DARK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(label, boxX + boxSize/2, boxY + boxSize/2, 2);
    }
}

void drawCheckmark(int16_t x, int16_t y, int16_t size, uint16_t color) {
    // Draw a simple checkmark using lines
    int16_t x1 = x - size/2;
    int16_t y1 = y;
    int16_t x2 = x - size/6;
    int16_t y2 = y + size/2;
    int16_t x3 = x + size/2;
    int16_t y3 = y - size/2;

    // Draw thick lines
    for (int i = -2; i <= 2; i++) {
        tft.drawLine(x1, y1 + i, x2, y2 + i, color);
        tft.drawLine(x2, y2 + i, x3, y3 + i, color);
    }
}

/*=========================SCREEN RENDERING FUNCTIONS=========================*/

void renderCurrentScreen() {
    switch (currentGUIState) {
        case GUI_SPLASH:
            drawSplashScreen();
            break;
        case GUI_HOME:
            drawHomeScreen();
            break;
        case GUI_SETTINGS:
            drawSettingsScreen();
            break;
        case GUI_FREQ_OVERRIDE:
            drawFreqOverrideScreen();
            break;
        case GUI_BASELINE_PROGRESS:
            drawProgressScreen(true);
            break;
        case GUI_BASELINE_COMPLETE:
            drawBaselineCompleteScreen();
            break;
        case GUI_FINAL_PROGRESS:
            drawProgressScreen(false);
            break;
        case GUI_RESULTS:
            drawResultsScreen();
            break;
    }
}

void drawSplashScreen() {
    // Clear screen with gradient background
    drawGradientRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_PRIMARY_START, COLOR_PRIMARY_END, false);

    // Draw "BioPal" text (logo will be added later)
    tft.setTextColor(COLOR_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("BioPal", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 20, 7);

    // Draw subtitle
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Impedance Analyzer", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 40, 2);

    // Note: Logo image will be added when converted to C array
}

void drawHomeScreen() {
    // Clear screen
    tft.fillScreen(COLOR_WHITE);

    // Draw header with gradient
    drawGradientRect(0, 0, SCREEN_WIDTH, 50, COLOR_PRIMARY_START, COLOR_PRIMARY_END, true);

    // Draw title
    tft.setTextColor(COLOR_WHITE);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("BioPal", 15, 25, 4);

    // Draw BLE connection indicator
    drawConnectionIndicator(SCREEN_WIDTH - 20, 25, isBLEConnected());

    // DUT selection area
    int16_t selectY = 70;
    tft.setTextColor(COLOR_TEXT_DARK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Number of DUTs", SCREEN_WIDTH/2, selectY, 2);

    // Large DUT count display
    char dutText[16];
    snprintf(dutText, sizeof(dutText), "%d", selectedDUTCount);
    tft.setTextColor(COLOR_PRIMARY_START);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(dutText, SCREEN_WIDTH/2, selectY + 40, 7);

    // Encoder hint
    tft.setTextColor(COLOR_TEXT_GRAY);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("< Rotate to adjust >", SCREEN_WIDTH/2, selectY + 75, 2);

    // Buttons
    int16_t btnY = 165;
    int16_t btnW = 130;
    int16_t btnH = 45;
    int16_t gap = 20;
    int16_t btn1X = (SCREEN_WIDTH - btnW * 2 - gap) / 2;
    int16_t btn2X = btn1X + btnW + gap;

    bool startHighlighted = (menuSelection == 0);
    bool settingsHighlighted = (menuSelection == 1);

    drawButton(btn1X, btnY, btnW, btnH, "START", startHighlighted, false);
    drawButton(btn2X, btnY, btnW, btnH, "SETTINGS", settingsHighlighted, false);
}

void drawSettingsScreen() {
    // Clear screen
    tft.fillScreen(COLOR_WHITE);

    // Draw header
    drawGradientRect(0, 0, SCREEN_WIDTH, 50, COLOR_PRIMARY_START, COLOR_PRIMARY_END, true);
    tft.setTextColor(COLOR_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Settings", SCREEN_WIDTH/2, 25, 4);

    // Menu items
    const int16_t itemY = 70;
    const int16_t itemH = 35;
    const int16_t itemGap = 5;

    // Item 0: Frequency Range
    int16_t y0 = itemY;
    bool highlighted0 = (menuSelection == 0);
    if (highlighted0) {
        tft.fillRect(10, y0, SCREEN_WIDTH - 20, itemH, COLOR_BG_MEDIUM);
    }
    tft.setTextColor(COLOR_TEXT_DARK);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("Freq Range:", 20, y0 + itemH/2, 2);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(guiSettings.useCustomFreqRange ? "Custom" : "Full", SCREEN_WIDTH - 20, y0 + itemH/2, 2);

    // Item 1: Back
    int16_t y1 = y0 + itemH + itemGap;
    bool highlighted1 = (menuSelection == 1);
    if (highlighted1) {
        tft.fillRect(10, y1, SCREEN_WIDTH - 20, itemH, COLOR_BG_MEDIUM);
    }
    tft.setTextDatum(ML_DATUM);
    tft.drawString("< Back to Home", 20, y1 + itemH/2, 2);

    // Instructions
    tft.setTextColor(COLOR_TEXT_GRAY);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Rotate: Navigate | Select: Toggle", SCREEN_WIDTH/2, SCREEN_HEIGHT - 20, 1);
}

void drawFreqOverrideScreen() {
    // Clear screen
    tft.fillScreen(COLOR_WHITE);

    // Draw header
    drawGradientRect(0, 0, SCREEN_WIDTH, 50, COLOR_PRIMARY_START, COLOR_PRIMARY_END, true);
    tft.setTextColor(COLOR_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Frequency Range", SCREEN_WIDTH/2, 25, 4);

    // Question
    tft.setTextColor(COLOR_TEXT_DARK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Use default range?", SCREEN_WIDTH/2, 80, 2);

    // Buttons
    int16_t btnY = 130;
    int16_t btnW = 130;
    int16_t btnH = 45;
    int16_t gap = 20;
    int16_t btn1X = (SCREEN_WIDTH - btnW * 2 - gap) / 2;
    int16_t btn2X = btn1X + btnW + gap;

    drawButton(btn1X, btnY, btnW, btnH, "DEFAULT", menuSelection == 0, false);
    drawButton(btn2X, btnY, btnW, btnH, "CUSTOM", menuSelection == 1, false);
}

void drawProgressScreen(bool isBaseline) {
    // Clear screen
    tft.fillScreen(COLOR_WHITE);

    // Draw header
    drawGradientRect(0, 0, SCREEN_WIDTH, 50, COLOR_PRIMARY_START, COLOR_PRIMARY_END, true);
    tft.setTextColor(COLOR_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(isBaseline ? "Baseline Measurement" : "Final Measurement", SCREEN_WIDTH/2, 25, 4);

    // Progress bar
    drawProgressBar(20, 70, SCREEN_WIDTH - 40, 30, progressPercent);

    // DUT status grid
    drawDUTStatusGrid(80, 120);

    // Current status text
    char statusText[32];
    if (progressPercent >= 100.0f) {
        snprintf(statusText, sizeof(statusText), "Complete!");
    } else if (currentDUT < totalDUTs) {
        snprintf(statusText, sizeof(statusText), "DUT %d/%d - Measuring...", currentDUT + 1, totalDUTs);
    } else {
        snprintf(statusText, sizeof(statusText), "Initializing...");
    }
    tft.setTextColor(COLOR_TEXT_DARK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(statusText, SCREEN_WIDTH/2, SCREEN_HEIGHT - 25, 2);
}

void drawBaselineCompleteScreen() {
    // Clear screen
    tft.fillScreen(COLOR_WHITE);

    // Draw header
    drawGradientRect(0, 0, SCREEN_WIDTH, 50, COLOR_PRIMARY_START, COLOR_PRIMARY_END, true);
    tft.setTextColor(COLOR_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Baseline Complete", SCREEN_WIDTH/2, 25, 4);

    // Draw large checkmark
    drawCheckmark(SCREEN_WIDTH/2, 110, 60, COLOR_SUCCESS);

    // Success message
    tft.setTextColor(COLOR_SUCCESS);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Baseline Done!", SCREEN_WIDTH/2, 150, 4);

    // Button
    drawButton(60, 185, SCREEN_WIDTH - 120, 45, "START FINAL", true, true);
}

void drawResultsScreen() {
    // Clear screen
    tft.fillScreen(COLOR_WHITE);

    // Draw header
    drawGradientRect(0, 0, SCREEN_WIDTH, 50, COLOR_PRIMARY_START, COLOR_PRIMARY_END, true);
    tft.setTextColor(COLOR_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Measurement Complete", SCREEN_WIDTH/2, 25, 4);

    // Draw large checkmark
    drawCheckmark(SCREEN_WIDTH/2, 110, 60, COLOR_SUCCESS);

    // Success message
    tft.setTextColor(COLOR_SUCCESS);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Done!", SCREEN_WIDTH/2, 145, 4);

    // Summary text
    char summaryText[32];
    snprintf(summaryText, sizeof(summaryText), "%d DUT%s tested", totalDUTs, totalDUTs > 1 ? "s" : "");
    tft.setTextColor(COLOR_TEXT_DARK);
    tft.drawString(summaryText, SCREEN_WIDTH/2, 175, 2);

    // Button
    drawButton(60, 195, SCREEN_WIDTH - 120, 40, "NEW TEST", true, false);
}
