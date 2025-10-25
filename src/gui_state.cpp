#include "gui_state.h"
#include "gui_screens.h"
#include "UART_Functions.h"
#include "defines.h"
#include <LittleFS.h>
#include <FS.h>

/*=========================STATE VARIABLES=========================*/

// Current GUI state
GUIState currentGUIState = GUI_SPLASH;

// Settings (saved to flash)
GUISettings guiSettings = {
    .useCustomFreqRange = false,  // Default: full range
    .startFreqIndex = 0,
    .endFreqIndex = 37,           // Full range by default
    .defaultDUTCount = 4
};

// User selections (current session)
uint8_t selectedDUTCount = 4;
uint8_t selectedStartFreq = 0;
uint8_t selectedEndFreq = 37;

// UI state
uint8_t menuSelection = 0;
bool menuEditMode = false;

// Progress tracking
uint8_t currentDUT = 0;
uint8_t totalDUTs = 0;
float progressPercent = 0.0f;
bool dutStatus[4] = {false, false, false, false};

// Button event queue
QueueHandle_t buttonEventQueue = nullptr;

// External measurement state variables (from main.cpp)
extern bool measurementInProgress;
extern bool baselineMeasurementDone;
extern bool finalMeasurementDone;
extern uint8_t num_duts;
extern uint8_t startIDX;
extern uint8_t endIDX;

/*=========================SETTINGS PERSISTENCE=========================*/

#define SETTINGS_FILE "/gui_settings.dat"

bool loadGUISettings() {
    if (!LittleFS.begin()) {
        Serial.println("[GUI] Failed to mount LittleFS");
        return false;
    }

    if (!LittleFS.exists(SETTINGS_FILE)) {
        Serial.println("[GUI] No saved settings found, using defaults");
        return false;
    }

    fs::File file = LittleFS.open(SETTINGS_FILE, "r");
    if (!file) {
        Serial.println("[GUI] Failed to open settings file");
        return false;
    }

    size_t bytesRead = file.read((uint8_t*)&guiSettings, sizeof(GUISettings));
    file.close();

    if (bytesRead != sizeof(GUISettings)) {
        Serial.println("[GUI] Settings file corrupted, using defaults");
        return false;
    }

    Serial.println("[GUI] Settings loaded from flash");
    selectedDUTCount = guiSettings.defaultDUTCount;
    return true;
}

bool saveGUISettings() {
    if (!LittleFS.begin()) {
        Serial.println("[GUI] Failed to mount LittleFS");
        return false;
    }

    fs::File file = LittleFS.open(SETTINGS_FILE, "w");
    if (!file) {
        Serial.println("[GUI] Failed to create settings file");
        return false;
    }

    size_t bytesWritten = file.write((uint8_t*)&guiSettings, sizeof(GUISettings));
    file.close();

    if (bytesWritten != sizeof(GUISettings)) {
        Serial.println("[GUI] Failed to write settings");
        return false;
    }

    Serial.println("[GUI] Settings saved to flash");
    return true;
}

/*=========================STATE MANAGEMENT=========================*/

void initGUIState() {
    // Create button event queue
    buttonEventQueue = xQueueCreate(10, sizeof(ButtonEvent));
    if (buttonEventQueue == nullptr) {
        Serial.println("[GUI] ERROR: Failed to create button event queue");
    }

    // Load settings from flash
    loadGUISettings();

    // Initialize state
    currentGUIState = GUI_SPLASH;
    menuSelection = 0;
    menuEditMode = false;

    Serial.println("[GUI] State machine initialized");
}

void setGUIState(GUIState newState) {
    if (newState == currentGUIState) {
        return;  // No change
    }

    Serial.printf("[GUI] State change: %d -> %d\n", currentGUIState, newState);

    // State exit actions
    switch (currentGUIState) {
        case GUI_SETTINGS:
            // Save settings when leaving settings screen
            saveGUISettings();
            break;
        default:
            break;
    }

    // Update state
    currentGUIState = newState;

    // State entry actions
    switch (newState) {
        case GUI_HOME:
            menuSelection = 0;  // Reset to START button
            break;

        case GUI_SETTINGS:
            menuSelection = 0;  // Reset to first menu item
            menuEditMode = false;
            break;

        case GUI_FREQ_OVERRIDE:
            menuSelection = 0;  // Default to "Use Default"
            break;

        case GUI_BASELINE_PROGRESS:
        case GUI_FINAL_PROGRESS:
            resetMeasurementTracking();
            break;

        default:
            break;
    }

    // Trigger screen redraw
    renderCurrentScreen();
}

GUIState getGUIState() {
    return currentGUIState;
}

QueueHandle_t getButtonEventQueue() {
    return buttonEventQueue;
}

/*=========================PROGRESS TRACKING=========================*/

void updateProgressScreen(uint8_t dutIndex) {
    if (dutIndex < MAX_DUT_COUNT) {
        dutStatus[dutIndex] = true;  // Mark DUT as complete

        // Update progress percentage
        uint8_t completedDUTs = 0;
        for (uint8_t i = 0; i < totalDUTs; i++) {
            if (dutStatus[i]) completedDUTs++;
        }
        progressPercent = ((float)completedDUTs / (float)totalDUTs) * 100.0f;

        Serial.printf("[GUI] Progress: DUT %d complete, %.0f%% done\n", dutIndex + 1, progressPercent);

        // Redraw progress screen
        if (currentGUIState == GUI_BASELINE_PROGRESS || currentGUIState == GUI_FINAL_PROGRESS) {
            renderCurrentScreen();
        }
    }
}

void resetMeasurementTracking() {
    currentDUT = 0;
    totalDUTs = selectedDUTCount;
    progressPercent = 0.0f;
    for (uint8_t i = 0; i < 4; i++) {
        dutStatus[i] = false;
    }
}

/*=========================INPUT HANDLING=========================*/

void handleGUIInput(ButtonEvent event) {
    Serial.printf("[GUI] Input event: %d in state %d\n", event, currentGUIState);

    switch (currentGUIState) {
        case GUI_SPLASH:
            // Any button skips splash screen
            if (event != BTN_EVENT_NONE) {
                setGUIState(GUI_HOME);
            }
            break;

        case GUI_HOME:
            if (event == BTN_EVENT_ROTATE_CW) {
                // Increase DUT count
                if (selectedDUTCount < 4) {
                    selectedDUTCount++;
                    renderCurrentScreen();
                }
            } else if (event == BTN_EVENT_ROTATE_CCW) {
                // Decrease DUT count
                if (selectedDUTCount > 1) {
                    selectedDUTCount--;
                    renderCurrentScreen();
                }
            } else if (event == BTN_EVENT_LEFT || event == BTN_EVENT_RIGHT) {
                // Toggle between START and SETTINGS buttons
                menuSelection = (menuSelection == 0) ? 1 : 0;
                renderCurrentScreen();
            } else if (event == BTN_EVENT_SELECT) {
                // Confirm selection
                if (menuSelection == 0) {
                    // START button - check if we need frequency override
                    if (guiSettings.useCustomFreqRange) {
                        setGUIState(GUI_FREQ_OVERRIDE);
                    } else {
                        // Start baseline measurement with default settings
                        num_duts = selectedDUTCount;
                        startIDX = guiSettings.startFreqIndex;
                        endIDX = guiSettings.endFreqIndex;
                        if (sendStartCommand(num_duts, startIDX, endIDX)) {
                            setGUIState(GUI_BASELINE_PROGRESS);
                        }
                    }
                } else {
                    // SETTINGS button
                    setGUIState(GUI_SETTINGS);
                }
            } else if (event == BTN_EVENT_RIGHT && menuSelection == 1) {
                // Quick access to settings
                setGUIState(GUI_SETTINGS);
            }
            break;

        case GUI_SETTINGS:
            if (event == BTN_EVENT_ROTATE_CW || event == BTN_EVENT_DOWN) {
                // Navigate down
                if (menuSelection < 1) {
                    menuSelection++;
                    renderCurrentScreen();
                }
            } else if (event == BTN_EVENT_ROTATE_CCW || event == BTN_EVENT_UP) {
                // Navigate up
                if (menuSelection > 0) {
                    menuSelection--;
                    renderCurrentScreen();
                }
            } else if (event == BTN_EVENT_SELECT) {
                if (menuSelection == 0) {
                    // Toggle frequency range setting
                    guiSettings.useCustomFreqRange = !guiSettings.useCustomFreqRange;
                    renderCurrentScreen();
                } else if (menuSelection == 1) {
                    // Back to home
                    setGUIState(GUI_HOME);
                }
            } else if (event == BTN_EVENT_LEFT) {
                // Back to home
                setGUIState(GUI_HOME);
            }
            break;

        case GUI_FREQ_OVERRIDE:
            if (event == BTN_EVENT_UP || event == BTN_EVENT_DOWN || event == BTN_EVENT_ROTATE_CW || event == BTN_EVENT_ROTATE_CCW) {
                // Toggle between default and custom
                menuSelection = (menuSelection == 0) ? 1 : 0;
                renderCurrentScreen();
            } else if (event == BTN_EVENT_SELECT) {
                if (menuSelection == 0) {
                    // Use default range
                    num_duts = selectedDUTCount;
                    startIDX = guiSettings.startFreqIndex;
                    endIDX = guiSettings.endFreqIndex;
                } else {
                    // Use custom range (for now, same as default - full implementation later)
                    num_duts = selectedDUTCount;
                    startIDX = guiSettings.startFreqIndex;
                    endIDX = guiSettings.endFreqIndex;
                }
                // Start baseline measurement
                if (sendStartCommand(num_duts, startIDX, endIDX)) {
                    setGUIState(GUI_BASELINE_PROGRESS);
                }
            } else if (event == BTN_EVENT_LEFT) {
                // Back to home
                setGUIState(GUI_HOME);
            }
            break;

        case GUI_BASELINE_PROGRESS:
        case GUI_FINAL_PROGRESS:
            if (event == BTN_EVENT_SELECT) {
                // Stop measurement (with confirmation in future)
                sendStopCommand();
                setGUIState(GUI_HOME);
            }
            break;

        case GUI_BASELINE_COMPLETE:
            if (event == BTN_EVENT_SELECT) {
                // Start final measurement
                if (sendStartCommand(num_duts, startIDX, endIDX)) {
                    setGUIState(GUI_FINAL_PROGRESS);
                }
            } else if (event == BTN_EVENT_LEFT) {
                // Back to home
                setGUIState(GUI_HOME);
            }
            break;

        case GUI_RESULTS:
            if (event == BTN_EVENT_SELECT) {
                // New measurement - reset and return to home
                baselineMeasurementDone = false;
                finalMeasurementDone = false;
                measurementInProgress = false;
                setGUIState(GUI_HOME);
            }
            break;
    }
}
