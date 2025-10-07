#include <Arduino.h>
#include <TFT_eSPI.h>
#include "uart_stm32.h"
#include "calibration.h"
#include "impedance_calc.h"
#include "bode_plot.h"

// Initialize modules
TFT_eSPI tft = TFT_eSPI();
UART_STM32 stm32_uart;
Calibration calibration;
ImpedanceCalculator impedance_calc(&calibration);
BodePlot bode_plot(&tft);

// State machine
enum MeasurementState {
    STATE_INIT,
    STATE_WAIT_BOOT,
    STATE_START_MEASUREMENT,
    STATE_WAIT_RESULTS,
    STATE_PROCESS_DUT1,
    STATE_PROCESS_DUT2,
    STATE_PROCESS_DUT3,
    STATE_DISPLAY_RESULTS,
    STATE_IDLE
};

MeasurementState current_state = STATE_INIT;
uint32_t state_start_time = 0;

// DUT results storage
DUTResults dut_results[3];
std::vector<ImpedancePoint> dut_impedance[3];

void setup() {
    // Initialize serial debug output
    Serial.begin(115200);
    while (!Serial) { }
    Serial.println("\n=== BioPal ESP32 Impedance Analyzer ===");

    // Initialize TFT display
    tft.init();
    tft.setRotation(0);
    Serial.println("TFT initialized");

    // Initialize Bode plot module
    if (!bode_plot.begin()) {
        Serial.println("ERROR: Failed to initialize Bode plot");
        return;
    }
    bode_plot.displayStatus("BioPal Initializing...");

    // Initialize UART communication with STM32
    if (!stm32_uart.begin()) {
        Serial.println("ERROR: Failed to initialize STM32 UART");
        bode_plot.displayStatus("UART Init Failed!");
        return;
    }

    // Initialize calibration module
    if (!calibration.begin()) {
        Serial.println("ERROR: Failed to initialize calibration");
        bode_plot.displayStatus("Calibration Failed!");
        return;
    }

    Serial.println("All modules initialized successfully");
    bode_plot.displayStatus("Ready!");

    // Transition to wait state
    current_state = STATE_WAIT_BOOT;
    state_start_time = millis();
    Serial.println("Waiting 5 seconds before starting measurement...");
}

void loop() {
    uint32_t current_time = millis();

    switch (current_state) {
        case STATE_INIT:
            // Should never reach here after setup
            break;

        case STATE_WAIT_BOOT:
            // Wait 5 seconds after boot before starting measurement
            if (current_time - state_start_time >= 5000) {
                Serial.println("5 second wait complete, starting measurement cycle");
                current_state = STATE_START_MEASUREMENT;
            }
            break;

        case STATE_START_MEASUREMENT:
            Serial.println("\n=== Starting Measurement Cycle ===");
            bode_plot.displayStatus("Measuring...");

            // Send START_MEASUREMENT command to STM32
            stm32_uart.sendStartMeasurement();

            current_state = STATE_WAIT_RESULTS;
            state_start_time = current_time;
            Serial.println("Waiting for measurement to complete...");
            break;

        case STATE_WAIT_RESULTS:
            // Wait briefly before starting to receive
            if (current_time - state_start_time > 1000) {
                Serial.println("Starting binary data reception...");
                current_state = STATE_PROCESS_DUT1;
            }
            break;

        case STATE_PROCESS_DUT1:
            Serial.println("Processing DUT 1...");
            if (stm32_uart.receiveDUTResults(1, dut_results[0])) {
                if (impedance_calc.calculateImpedance(dut_results[0], dut_impedance[0])) {
                    Serial.println("DUT 1 impedance calculated successfully");
                    impedance_calc.printImpedance();
                }
                current_state = STATE_PROCESS_DUT2;
            } else {
                Serial.println("ERROR: Failed to receive DUT 1 results");
                bode_plot.displayStatus("DUT 1 Timeout!");
                current_state = STATE_IDLE;
            }
            break;

        case STATE_PROCESS_DUT2:
            Serial.println("Processing DUT 2...");
            if (stm32_uart.receiveDUTResults(2, dut_results[1])) {
                if (impedance_calc.calculateImpedance(dut_results[1], dut_impedance[1])) {
                    Serial.println("DUT 2 impedance calculated successfully");
                    impedance_calc.printImpedance();
                }
                current_state = STATE_PROCESS_DUT3;
            } else {
                Serial.println("ERROR: Failed to receive DUT 2 results");
                bode_plot.displayStatus("DUT 2 Timeout!");
                current_state = STATE_IDLE;
            }
            break;

        case STATE_PROCESS_DUT3:
            Serial.println("Processing DUT 3...");
            if (stm32_uart.receiveDUTResults(3, dut_results[2])) {
                if (impedance_calc.calculateImpedance(dut_results[2], dut_impedance[2])) {
                    Serial.println("DUT 3 impedance calculated successfully");
                    impedance_calc.printImpedance();
                }
                current_state = STATE_DISPLAY_RESULTS;
            } else {
                Serial.println("ERROR: Failed to receive DUT 3 results");
                bode_plot.displayStatus("DUT 3 Timeout!");
                current_state = STATE_IDLE;
            }
            break;

        case STATE_DISPLAY_RESULTS:
            Serial.println("\n=== Displaying Results ===");

            // Display DUT 1 results (or cycle through all 3 DUTs)
            // For now, just display DUT 1
            if (dut_impedance[0].size() > 0) {
                bode_plot.plotImpedance(dut_impedance[0], 1);
                Serial.println("DUT 1 Bode plot displayed");
            } else {
                bode_plot.displayStatus("No Valid Data");
            }

            current_state = STATE_IDLE;
            Serial.println("Measurement cycle complete");
            break;

        case STATE_IDLE:
            // Stay in idle state
            // Could add button press to restart measurement here
            delay(100);
            break;
    }
}
