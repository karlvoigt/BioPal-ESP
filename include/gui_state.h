#ifndef GUI_STATE_H
#define GUI_STATE_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/*=========================GUI STATE DEFINITIONS=========================*/

// GUI state machine states
enum GUIState {
    GUI_SPLASH,              // Initial splash screen with logo
    GUI_HOME,                // Home screen: DUT selection + start button
    GUI_SETTINGS,            // Settings menu
    GUI_FREQ_OVERRIDE,       // One-time frequency override screen
    GUI_BASELINE_PROGRESS,   // Baseline measurement in progress
    GUI_BASELINE_COMPLETE,   // Baseline measurement complete
    GUI_FINAL_PROGRESS,      // Final measurement in progress
    GUI_RESULTS              // Measurement complete / results
};

// Button/encoder events
enum ButtonEvent {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_UP,
    BTN_EVENT_DOWN,
    BTN_EVENT_LEFT,
    BTN_EVENT_RIGHT,
    BTN_EVENT_SELECT,
    BTN_EVENT_ROTATE_CW,    // Rotary encoder clockwise
    BTN_EVENT_ROTATE_CCW    // Rotary encoder counter-clockwise
};

// Settings structure
struct GUISettings {
    bool useCustomFreqRange;  // false = full range, true = custom
    uint8_t startFreqIndex;   // Index into frequency table
    uint8_t endFreqIndex;     // Index into frequency table
    uint8_t defaultDUTCount;  // Default number of DUTs (1-4)
};

/*=========================GUI STATE VARIABLES=========================*/

// Current GUI state
extern GUIState currentGUIState;

// GUI settings (persisted to flash)
extern GUISettings guiSettings;

// User selections (current session)
extern uint8_t selectedDUTCount;    // Current DUT count selection (1-4)
extern uint8_t selectedStartFreq;   // Temporary freq selection
extern uint8_t selectedEndFreq;     // Temporary freq selection

// UI state
extern uint8_t menuSelection;       // Currently highlighted menu item
extern bool menuEditMode;           // true when editing a value

// Progress tracking for UI
extern uint8_t currentDUT;          // Current DUT being measured (0-3)
extern uint8_t totalDUTs;           // Total DUTs in this measurement
extern float progressPercent;       // Overall progress (0.0 - 100.0)
extern bool dutStatus[4];           // Status of each DUT (false=pending, true=complete)

/*=========================GUI STATE FUNCTIONS=========================*/

// Initialize GUI state machine
void initGUIState();

// Set new GUI state and trigger screen redraw
void setGUIState(GUIState newState);

// Get current GUI state
GUIState getGUIState();

// Handle button/encoder input based on current state
void handleGUIInput(ButtonEvent event);

// Update progress display (called when DUT completes)
void updateProgressScreen(uint8_t dutIndex);

// Reset measurement tracking
void resetMeasurementTracking();

// Load settings from flash
bool loadGUISettings();

// Save settings to flash
bool saveGUISettings();

// Get reference to button event queue
QueueHandle_t getButtonEventQueue();

#endif // GUI_STATE_H
