#include "gui_screens.h"
#include "defines.h"
#include <LittleFS.h>
#include "logo.h"

// TFT instance (shared with bode_plot.cpp)
extern TFT_eSPI tft;

// Sprite for double buffering (full screen)
TFT_eSprite sprite = TFT_eSprite(&tft);

// PNG rendering position
int16_t png_xpos = 0;
int16_t png_ypos = 0;

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

/*=========================SPRITE INITIALIZATION=========================*/

bool initSpriteBuffer() {
    Serial.println("[GUI] Initializing sprite buffer...");

    // Print initial heap stats
    printHeapStats();

    // Create sprite buffer (320x240x2 = 153,600 bytes)
    bool success = sprite.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);

    if (success) {
        Serial.println("[GUI] Sprite buffer created successfully!");
        Serial.printf("[GUI] Sprite size: %d x %d = %d bytes\n",
            SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH * SCREEN_HEIGHT * 2);
        printHeapStats();
    } else {
        Serial.println("[GUI] ERROR: Failed to create sprite buffer!");
        Serial.println("[GUI] Falling back to direct rendering (will have flicker)");
    }

    return success;
}

void printHeapStats() {
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t heapSize = ESP.getHeapSize();
    uint32_t usedHeap = heapSize - freeHeap;
    float usedPercent = (float)usedHeap / (float)heapSize * 100.0f;

    Serial.printf("[HEAP] Total: %d bytes, Used: %d bytes (%.1f%%), Free: %d bytes\n",
        heapSize, usedHeap, usedPercent, freeHeap);
}


/*=========================HELPER DRAWING FUNCTIONS=========================*/

void drawGradientRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color1, uint16_t color2, bool horizontal) {
    if (horizontal) {
        // Horizontal gradient
        for (int16_t i = 0; i < w; i++) {
            float t = (float)i / (float)w;
            uint16_t color = lerpColor(color1, color2, t);
            sprite.drawFastVLine(x + i, y, h, color);
        }
    } else {
        // Vertical gradient
        for (int16_t i = 0; i < h; i++) {
            float t = (float)i / (float)h;
            uint16_t color = lerpColor(color1, color2, t);
            sprite.drawFastHLine(x, y + i, w, color);
        }
    }
}

void drawCenteredText(const char* text, int16_t y, uint8_t font, uint16_t color) {
    sprite.setTextColor(color);
    sprite.setTextDatum(TC_DATUM);  // Top center
    sprite.drawString(text, SCREEN_WIDTH / 2, y, font);
}

void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t fillColor, uint16_t borderColor) {
    sprite.fillRoundRect(x, y, w, h, r, fillColor);
    if (borderColor != fillColor) {
        sprite.drawRoundRect(x, y, w, h, r, borderColor);
    }
}

void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char* text, bool highlighted, bool large) {
    uint16_t fillColor, textColor;

    if (highlighted) {
        // Solid fill for highlighted button with rounded corners
        sprite.fillRoundRect(x, y, w, h, 8, COLOR_PRIMARY_START);
        textColor = COLOR_WHITE;
    } else {
        fillColor = COLOR_BG_MEDIUM;
        textColor = COLOR_TEXT_DARK;
        drawRoundRect(x, y, w, h, 8, fillColor, fillColor);
    }

    // Draw text centered in button
    sprite.setTextColor(textColor);
    sprite.setTextDatum(MC_DATUM);  // Middle center
    sprite.drawString(text, x + w/2, y + h/2, large ? 4 : 2);
}

void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, float percent) {
    // Background
    sprite.fillRoundRect(x, y, w, h, h/2, COLOR_BG_MEDIUM);

    // Calculate fill width
    int16_t fillWidth = (int16_t)((float)w * percent / 100.0f);
    if (fillWidth > 0) {
        // Draw gradient fill
        drawGradientRect(x, y, fillWidth, h, COLOR_PRIMARY_START, COLOR_PRIMARY_END, true);

        // Draw percentage text in center
        char percentText[8];
        snprintf(percentText, sizeof(percentText), "%.0f%%", percent);
        sprite.setTextColor(COLOR_WHITE);
        sprite.setTextDatum(MC_DATUM);
        sprite.drawString(percentText, x + w/2, y + h/2, 2);
    }
}

void drawConnectionIndicator(int16_t x, int16_t y, bool connected) {
    uint16_t color = connected ? COLOR_SUCCESS : COLOR_DANGER;
    sprite.fillCircle(x, y, 5, color);

    // Add subtle glow effect for connected state
    if (connected) {
        sprite.drawCircle(x, y, 7, lerpColor(COLOR_SUCCESS, COLOR_BG_LIGHT, 0.5));
    }
}

void drawConnectionIndicatorDefault(bool connected) {
    drawConnectionIndicator(SCREEN_WIDTH - 20, 25, connected);
}

void drawDUTStatusGrid(int16_t x, int16_t y) {
    const int16_t boxSize = 60;
    const int16_t gap = 10;
    const int16_t cols = 4;

    for (uint8_t i = 0; i < totalDUTs; i++) {
        int16_t boxX = x - (cols * boxSize + (cols - 1) * gap) / 2 + (i) * (boxSize + gap);
        int16_t boxY = y;

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
        sprite.setTextColor(COLOR_TEXT_DARK);
        sprite.setTextDatum(MC_DATUM);
        sprite.drawString("Sensor", boxX + boxSize/2, boxY + boxSize/2 - 10, 2);
        
        char numLabel[4];
        snprintf(numLabel, sizeof(numLabel), "%d", i + 1);
        sprite.drawString(numLabel, boxX + boxSize/2, boxY + boxSize/2 + 10, 2);
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
        sprite.drawLine(x1, y1 + i, x2, y2 + i, color);
        sprite.drawLine(x2, y2 + i, x3, y3 + i, color);
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
    // drawGradientRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_PRIMARY_START, COLOR_PRIMARY_END, false);

    // sprite.pushSprite(0, 0);  
    // Display logo centered                       │
    int16_t x = (SCREEN_WIDTH - LOGO_WIDTH) / 2;   
    int16_t y = 0;  // Position from top          
    tft.setSwapBytes(true);
    tft.pushImage(x, y, LOGO_WIDTH, LOGO_HEIGHT,logo);    
    tft.setSwapBytes(false);                                        
                                                    
    // Subtitle                                    
    // sprite.setTextColor(COLOR_WHITE);              
    // sprite.setTextDatum(TC_DATUM);                 
    // sprite.drawString("Impedance Analyzer",SCREEN_WIDTH/2, SCREEN_HEIGHT - 40, 2);            
             
}

void drawHomeScreen() {
    // Clear screen
    sprite.fillSprite(COLOR_WHITE);

    // Draw header with gradient
    drawGradientRect(0, 0, SCREEN_WIDTH, 50, COLOR_PRIMARY_START, COLOR_PRIMARY_END, true);

    // Draw title
    sprite.setTextColor(COLOR_WHITE);
    sprite.setTextDatum(ML_DATUM);
    sprite.drawString("BioPal", 15, 25, 4);

    // Draw BLE connection indicator
    drawConnectionIndicator(SCREEN_WIDTH - 20, 25, isBLEConnected());

    // DUT selection area
    int16_t selectY = 70;
    sprite.setTextColor(COLOR_TEXT_DARK);
    sprite.setTextDatum(TC_DATUM);
    sprite.drawString("Number of Sensors", SCREEN_WIDTH/2, selectY, 2);

    // Large DUT count display
    char dutText[16];
    snprintf(dutText, sizeof(dutText), "%d", selectedDUTCount);
    sprite.setTextColor(COLOR_PRIMARY_START);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString(dutText, SCREEN_WIDTH/2, selectY + 40, 7);

    // Encoder hint
    sprite.setTextColor(COLOR_TEXT_GRAY);
    sprite.setTextDatum(TC_DATUM);
    sprite.drawString("< Rotate to adjust >", SCREEN_WIDTH/2, selectY + 75, 2);

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

    // Push sprite to screen
    sprite.pushSprite(0, 0);
}

void drawSettingsScreen() {
    // Clear screen
    sprite.fillSprite(COLOR_WHITE);

    // Draw header
    drawGradientRect(0, 0, SCREEN_WIDTH, 50, COLOR_PRIMARY_START, COLOR_PRIMARY_END, true);
    sprite.setTextColor(COLOR_WHITE);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString("Settings", SCREEN_WIDTH/2, 25, 4);

    // Menu items
    const int16_t itemY = 70;
    const int16_t itemH = 35;
    const int16_t itemGap = 5;

    // Item 0: Frequency Range
    int16_t y0 = itemY;
    bool highlighted0 = (menuSelection == 0);
    if (highlighted0) {
        sprite.fillRect(10, y0, SCREEN_WIDTH - 20, itemH, COLOR_BG_MEDIUM);
    }
    sprite.setTextColor(COLOR_TEXT_DARK);
    sprite.setTextDatum(ML_DATUM);
    sprite.drawString("Freq Range:", 20, y0 + itemH/2, 2);
    sprite.setTextDatum(MR_DATUM);
    sprite.drawString(guiSettings.useCustomFreqRange ? "Custom" : "Full", SCREEN_WIDTH - 20, y0 + itemH/2, 2);

    // Item 1: Back
    int16_t y1 = y0 + itemH + itemGap;
    bool highlighted1 = (menuSelection == 1);
    if (highlighted1) {
        sprite.fillRect(10, y1, SCREEN_WIDTH - 20, itemH, COLOR_BG_MEDIUM);
    }
    sprite.setTextDatum(ML_DATUM);
    sprite.drawString("< Back to Home", 20, y1 + itemH/2, 2);

    // Instructions
    sprite.setTextColor(COLOR_TEXT_GRAY);
    sprite.setTextDatum(TC_DATUM);
    sprite.drawString("Rotate: Navigate | Select: Toggle", SCREEN_WIDTH/2, SCREEN_HEIGHT - 20, 1);

    // Push sprite to screen
    sprite.pushSprite(0, 0);
}

void drawFreqOverrideScreen() {
    // Clear screen
    sprite.fillSprite(COLOR_WHITE);

    // Draw header
    drawGradientRect(0, 0, SCREEN_WIDTH, 50, COLOR_PRIMARY_START, COLOR_PRIMARY_END, true);
    sprite.setTextColor(COLOR_WHITE);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString("Frequency Range", SCREEN_WIDTH/2, 25, 4);

    // Question
    sprite.setTextColor(COLOR_TEXT_DARK);
    sprite.setTextDatum(TC_DATUM);
    sprite.drawString("Use default range?", SCREEN_WIDTH/2, 80, 2);

    // Buttons
    int16_t btnY = 130;
    int16_t btnW = 130;
    int16_t btnH = 45;
    int16_t gap = 20;
    int16_t btn1X = (SCREEN_WIDTH - btnW * 2 - gap) / 2;
    int16_t btn2X = btn1X + btnW + gap;

    drawButton(btn1X, btnY, btnW, btnH, "DEFAULT", menuSelection == 0, false);
    drawButton(btn2X, btnY, btnW, btnH, "CUSTOM", menuSelection == 1, false);

    // Push sprite to screen
    sprite.pushSprite(0, 0);
}

void drawProgressScreen(bool isBaseline) {
    // Clear screen
    sprite.fillSprite(COLOR_WHITE);

    // Draw header
    drawGradientRect(0, 0, SCREEN_WIDTH, 50, COLOR_PRIMARY_START, COLOR_PRIMARY_END, true);
    sprite.setTextColor(COLOR_WHITE);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString(isBaseline ? "Baseline Measurement" : "Final Measurement", SCREEN_WIDTH/2, 25, 4);

    // Progress bar
    drawProgressBar(20, 70, SCREEN_WIDTH - 40, 30, progressPercent);

    // DUT status grid
    drawDUTStatusGrid(160, 120);

    // Current status text
    char statusText[32];
    if (progressPercent >= 100.0f) {
        snprintf(statusText, sizeof(statusText), "Complete!");
    } else if (currentDUT < totalDUTs) {
        snprintf(statusText, sizeof(statusText), "Sensor %d/%d - Measuring...", currentDUT + 1, totalDUTs);
    } else {
        snprintf(statusText, sizeof(statusText), "Initializing...");
    }
    sprite.setTextColor(COLOR_TEXT_DARK);
    sprite.setTextDatum(TC_DATUM);
    sprite.drawString(statusText, SCREEN_WIDTH/2, SCREEN_HEIGHT - 25, 2);

    // Push sprite to screen
    sprite.pushSprite(0, 0);
}

void drawBaselineCompleteScreen() {
    // Clear screen
    sprite.fillSprite(COLOR_WHITE);

    // Draw header
    drawGradientRect(0, 0, SCREEN_WIDTH, 50, COLOR_PRIMARY_START, COLOR_PRIMARY_END, true);
    sprite.setTextColor(COLOR_WHITE);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString("Baseline Complete", SCREEN_WIDTH/2, 25, 4);

    // Draw large checkmark
    drawCheckmark(SCREEN_WIDTH/2, 110, 60, COLOR_SUCCESS);

    // Success message
    sprite.setTextColor(COLOR_SUCCESS);
    sprite.setTextDatum(TC_DATUM);
    // sprite.drawString("Baseline Done!", SCREEN_WIDTH/2, 150, 4);

    // Button
    drawButton(60, 185, SCREEN_WIDTH - 120, 45, "START FINAL", true, true);

    // Push sprite to screen
    sprite.pushSprite(0, 0);
}

void drawResultsScreen() {
    // Clear screen
    sprite.fillSprite(COLOR_WHITE);

    // Draw header
    drawGradientRect(0, 0, SCREEN_WIDTH, 50, COLOR_PRIMARY_START, COLOR_PRIMARY_END, true);
    sprite.setTextColor(COLOR_WHITE);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString("Measurement Complete", SCREEN_WIDTH/2, 25, 4);

    // Draw large checkmark
    drawCheckmark(SCREEN_WIDTH/2, 110, 60, COLOR_SUCCESS);

    // Success message
    sprite.setTextColor(COLOR_SUCCESS);
    sprite.setTextDatum(TC_DATUM);
    sprite.drawString("Done!", SCREEN_WIDTH/2, 145, 4);

    // Summary text
    char summaryText[32];
    snprintf(summaryText, sizeof(summaryText), "%d Sensor%s tested", totalDUTs, totalDUTs > 1 ? "s" : "");
    sprite.setTextColor(COLOR_TEXT_DARK);
    sprite.drawString(summaryText, SCREEN_WIDTH/2, 175, 2);

    // Button
    drawButton(60, 195, SCREEN_WIDTH - 120, 40, "NEW TEST", true, false);

    // Push sprite to screen
    sprite.pushSprite(0, 0);
}
