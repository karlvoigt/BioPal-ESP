#include "button_handler.h"

/*=========================BUTTON STATE TRACKING=========================*/

// Last interrupt time for debouncing
static volatile unsigned long lastInterruptTime[5] = {0};

// Encoder state tracking
static volatile int8_t encoderPos = 0;
static volatile uint8_t encoderState = 0;
static volatile int8_t lastEncoderPos = 0;

// Button event queue (defined in gui_state.cpp)
extern QueueHandle_t buttonEventQueue;

/*=========================INTERRUPT SERVICE ROUTINES=========================*/

// Generic button ISR with debouncing
void IRAM_ATTR buttonISR(uint8_t buttonIndex, ButtonEvent event) {
    unsigned long now = millis();

    // Debounce check
    if (now - lastInterruptTime[buttonIndex] < BUTTON_DEBOUNCE_MS) {
        return;
    }
    lastInterruptTime[buttonIndex] = now;

    // Send event to queue (ISR-safe)
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(buttonEventQueue, &event, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// Individual button ISRs
void IRAM_ATTR btnUpISR() {
    buttonISR(0, BTN_EVENT_UP);
}

void IRAM_ATTR btnDownISR() {
    buttonISR(1, BTN_EVENT_DOWN);
}

void IRAM_ATTR btnLeftISR() {
    buttonISR(2, BTN_EVENT_LEFT);
}

void IRAM_ATTR btnRightISR() {
    buttonISR(3, BTN_EVENT_RIGHT);
}

void IRAM_ATTR btnSelectISR() {
    buttonISR(4, BTN_EVENT_SELECT);
}

// Rotary encoder ISR (quadrature decoding)
// Based on Gray code state machine
void IRAM_ATTR encoderISR() {
    // Read current encoder pins
    uint8_t A = digitalRead(ENCODER_A);
    uint8_t B = digitalRead(ENCODER_B);

    // Combine into 2-bit value
    uint8_t currentState = (A << 1) | B;

    // Create 4-bit value: old_state | new_state
    uint8_t combinedState = (encoderState << 2) | currentState;

    // Update encoder state
    encoderState = currentState;

    // Quadrature decoding using lookup table
    // CW: 0b0001, 0b0111, 0b1110, 0b1000
    // CCW: 0b0010, 0b1011, 0b1101, 0b0100
    switch (combinedState) {
        case 0b0001: // CW step 1
        case 0b0111: // CW step 2
        case 0b1110: // CW step 3
        case 0b1000: // CW step 4
            encoderPos = encoderPos + 1;
            break;

        case 0b0010: // CCW step 1
        case 0b1011: // CCW step 2
        case 0b1101: // CCW step 3
        case 0b0100: // CCW step 4
            encoderPos = encoderPos - 1;
            break;

        default:
            // Invalid state or no movement
            break;
    }

    // Check if we've moved enough to trigger an event
    int8_t delta = encoderPos - lastEncoderPos;
    if (abs(delta) >= ENCODER_PULSES_PER_DETENT) {
        ButtonEvent event = (delta > 0) ? BTN_EVENT_ROTATE_CW : BTN_EVENT_ROTATE_CCW;
        lastEncoderPos = encoderPos;

        // Send event to queue (ISR-safe)
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(buttonEventQueue, &event, &xHigherPriorityTaskWoken);

        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

/*=========================PUBLIC FUNCTIONS=========================*/

void initButtons() {
    Serial.println("[BTN] Initializing button interrupts...");

    // Configure button pins as inputs with pull-ups
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_LEFT, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);

    // Configure encoder pins as inputs with pull-ups
    pinMode(ENCODER_A, INPUT_PULLUP);
    pinMode(ENCODER_B, INPUT_PULLUP);

    // Read initial encoder state
    uint8_t A = digitalRead(ENCODER_A);
    uint8_t B = digitalRead(ENCODER_B);
    encoderState = (A << 1) | B;

    // Attach interrupts (active LOW - triggers on press)
    attachInterrupt(digitalPinToInterrupt(BTN_UP), btnUpISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(BTN_DOWN), btnDownISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(BTN_LEFT), btnLeftISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(BTN_RIGHT), btnRightISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(BTN_SELECT), btnSelectISR, FALLING);

    // Attach encoder interrupts (trigger on any change)
    attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_B), encoderISR, CHANGE);

    Serial.println("[BTN] Button interrupts initialized");
}

void disableButtons() {
    detachInterrupt(digitalPinToInterrupt(BTN_UP));
    detachInterrupt(digitalPinToInterrupt(BTN_DOWN));
    detachInterrupt(digitalPinToInterrupt(BTN_LEFT));
    detachInterrupt(digitalPinToInterrupt(BTN_RIGHT));
    detachInterrupt(digitalPinToInterrupt(BTN_SELECT));
    detachInterrupt(digitalPinToInterrupt(ENCODER_A));
    detachInterrupt(digitalPinToInterrupt(ENCODER_B));
}

void enableButtons() {
    attachInterrupt(digitalPinToInterrupt(BTN_UP), btnUpISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(BTN_DOWN), btnDownISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(BTN_LEFT), btnLeftISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(BTN_RIGHT), btnRightISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(BTN_SELECT), btnSelectISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_B), encoderISR, CHANGE);
}

bool isButtonPressed(uint8_t buttonPin) {
    return digitalRead(buttonPin) == LOW;  // Active LOW
}
