#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include "gui_state.h"
#include "pinDefs.h"

/*=========================BUTTON HANDLER CONFIGURATION=========================*/

// Debounce time in milliseconds
#define BUTTON_DEBOUNCE_MS 250

// Encoder configuration
#define ENCODER_PULSES_PER_DETENT 2  // Adjust based on encoder type

/*=========================BUTTON HANDLER FUNCTIONS=========================*/

// Initialize button interrupts and encoder
// Must be called before using button inputs
void initButtons();

// Disable button interrupts (for power saving or during critical sections)
void disableButtons();

// Enable button interrupts
void enableButtons();

// Check if any button is currently pressed (for long-press detection, etc.)
bool isButtonPressed(uint8_t buttonPin);

#endif // BUTTON_HANDLER_H
