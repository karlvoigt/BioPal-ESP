#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "defines.h"
#include "UART_Functions.h"
#include "calibration.h"
#include "impedance_calc.h"
#include "bode_plot.h"
#include "csv_export.h"
#include "serial_commands.h"
#include "BLE_Functions.h"

/*=========================GLOBAL VARIABLES=========================*/
// Initialize global impedance data arrays
ImpedancePoint baselineImpedanceData[MAX_DUT_COUNT][MAX_FREQUENCIES];
ImpedancePoint measurementImpedanceData[MAX_DUT_COUNT][MAX_FREQUENCIES];
int frequencyCount[MAX_DUT_COUNT] = {0};
bool measurementInProgress = false;
bool baselineMeasurementDone = false;
bool finalMeasurementDone = false;
uint8_t startIDX = 0;
uint8_t endIDX = 37;
uint8_t num_duts = 1;

// FreeRTOS queue for measurement data
QueueHandle_t measurementQueue;

/*=========================TASK: UART READER=========================*/
// Task to process bytes from circular buffer
// ISR just collects bytes - this task does the heavy state machine work
void taskUARTReader(void* parameter) {
    Serial.println("UART Reader task started");

    SemaphoreHandle_t uartSem = getUARTSemaphore();

    while (true) {
        // Wait for semaphore from ISR (signals data was received)
        // This task sleeps until interrupt occurs - no polling!
        if (xSemaphoreTake(uartSem, pdMS_TO_TICKS(10000)) == pdTRUE) {
            // Process all buffered bytes from ISR circular buffer
            // This runs in task context with full stack - safe!
            processBufferedBytes();
        } else {
            // Timeout - no data for 10 seconds (normal during idle)
        }
    }
}

/*=========================TASK: DATA PROCESSOR=========================*/
// Task to receive measurements, calibrate, calculate impedance, and store
void taskDataProcessor(void* parameter) {
    MeasurementPoint point;
    Serial.println("Data Processor task started");

    while (true) {
        // Wait for measurement point from queue
        if (xQueueReceive(measurementQueue, &point, portMAX_DELAY) == pdTRUE) {
            // Get current DUT number (1-4 from STM, convert to 0-3 for array)
            uint8_t dutIndex = getCurrentDUT() - 1;

            if (dutIndex >= MAX_DUT_COUNT) {
                Serial.printf("ERROR: Invalid DUT index %d\n", dutIndex + 1);
                continue;
            }


            // Calculate impedance: Z = V / I
            ImpedancePoint impedance = calcImpedance(point);

            // Calibrate the measurement point
            if (calibrate(impedance)) {
                Serial.printf("Calibrated: Z=%.6e Phase=%.2f\n",
                             impedance.Z_magnitude, impedance.Z_phase);
            } else {
                Serial.printf("WARNING: Calibration failed for freq=%lu Hz\n", point.freq_hz);
            }

            // Store in global impedance array
            int freqIndex = frequencyCount[dutIndex];
            if (freqIndex < MAX_FREQUENCIES) {
                if (baselineMeasurementDone) {
                    measurementImpedanceData[dutIndex][freqIndex] = impedance;
                } else {
                    baselineImpedanceData[dutIndex][freqIndex] = impedance;
                }
                frequencyCount[dutIndex]++;
            } else {
                Serial.printf("ERROR: Frequency buffer full for DUT %d\n", dutIndex + 1);
            }
        }
    }
}

/*=========================BLE COMMAND PROCESSING=========================*/
void processBLECommands() {
    char cmdBuffer[64];

    // Check for BLE command
    if (!getBLECommand(cmdBuffer, sizeof(cmdBuffer))) {
        return;  // No command available
    }

    Serial.printf("[BLE] Processing command: '%s'\n", cmdBuffer);

    String cmdStr(cmdBuffer);

    // Parse START command
    if (cmdStr.startsWith(BLE_CMD_BASELINE)) {
        if (measurementInProgress) {
            sendBLEError("Measurement already in progress");
            return;
        } else if (baselineMeasurementDone && !finalMeasurementDone) {
            sendBLEError("Baseline measurement already done, proceed to MEAS");
            return;
        }
        baselineMeasurementDone = false;
        finalMeasurementDone = false;
        
        parseStartCommand(cmdBuffer, num_duts, startIDX, endIDX);

        if (num_duts == 0) {
            sendBLEError("Invalid DUT count (must be 1-4)");
            return;
        }

        Serial.printf("[BLE] Starting Baseline measurement with %d DUT%s...\n", num_duts, num_duts > 1 ? "s" : "");

        // Clear previous measurement data
        for (int i = 0; i < MAX_DUT_COUNT; i++) {
            frequencyCount[i] = 0;
            for (int j = 0; j < MAX_FREQUENCIES; j++) {
                baselineImpedanceData[i][j] = ImpedancePoint();
            }
        }
        Serial.println("[BLE] Buffers cleared - ready for new measurement");

        // Start measurement via UART
        if (sendStartCommand(num_duts, startIDX, endIDX)) {
            // Send status update
            sendBLEStatus("Measuring");
            measurementInProgress = true;
        } else {
            sendBLEError("Failed to start measurement");
        }

    }
    else if (cmdStr.equals(BLE_CMD_MEAS)) {
        if (measurementInProgress) {
            sendBLEError("Measurement already in progress");
            return;
        } else if (!baselineMeasurementDone) {
            sendBLEError("Baseline measurement needs to be done first");
            return;
        }

        for (int i = 0; i < MAX_DUT_COUNT; i++) {
            frequencyCount[i] = 0;
            for (int j = 0; j < MAX_FREQUENCIES; j++) {
                measurementImpedanceData[i][j] = ImpedancePoint();
            }
        }
        finalMeasurementDone = false;
        // Start measurement via UART
        if (sendStartCommand(num_duts, startIDX, endIDX)) {
            // Send status update
            sendBLEStatus("Measuring");
            measurementInProgress = true;
        } else {
            sendBLEError("Failed to start measurement");
        }
    }
    // Parse STOP command
    else if (cmdStr.equals(BLE_CMD_STOP)) {
        Serial.println("[BLE] Stopping measurement...");
        sendStopCommand();
        sendBLEStatus("Stopped");
        measurementInProgress = false;
    }
    else {
        Serial.printf("[BLE] ERROR: Unknown command '%s'\n", cmdBuffer);
        sendBLEError("Unknown command");
    }
}

/*=========================TASK: GUI=========================*/
// Task to handle GUI and user interaction
void taskGUI(void* parameter) {
    Serial.println("GUI task started");

    // Initialize TFT display
    initBodePlot();
    Serial.println("TFT display initialized");

    // Wait a bit for system to stabilize
    vTaskDelay(pdMS_TO_TICKS(2000));

    Serial.println("\n=== BioPal ESP32 Ready ===");
    Serial.println("Type 'help' for available commands\n");

    // Get semaphore handles
    SemaphoreHandle_t dutCompleteSem = getDUTCompleteSemaphore();
    SemaphoreHandle_t measurementCompleteSem = getMeasurementCompleteSemaphore();

    bool allMeasurementsComplete = false;

    // Main GUI event loop
    while (true) {
        // Process serial commands from computer
        processSerialCommands();

        // Process BLE commands from WebUI
        processBLECommands();

        // Wait for DUT completion event (100ms timeout to allow command processing)
        if (xSemaphoreTake(dutCompleteSem, pdMS_TO_TICKS(100)) == pdTRUE) {
            // DUT just completed - draw Bode plot
            uint8_t dutIndex = getCompletedDUTIndex();
            Serial.printf("Drawing Bode plot for completed DUT %d...\n", dutIndex + 1);
            drawBodePlot(dutIndex);

            // Send DUT start notification via BLE
            sendBLEDUTStart(dutIndex + 1);

            // Send impedance data via BLE
            if (sendBLEImpedanceData(dutIndex)) {
                Serial.printf("[BLE] Sent data for DUT %d\n", dutIndex + 1);
            }

            // Send DUT end notification via BLE
            sendBLEDUTEnd(dutIndex + 1);

            // Check if all measurements are complete
            if (xSemaphoreTake(measurementCompleteSem, 0) == pdTRUE) {
                allMeasurementsComplete = true;
                measurementInProgress = false;
                if (!baselineMeasurementDone) {
                    baselineMeasurementDone = true;
                    Serial.println("Baseline measurement completed");
                } else {
                    finalMeasurementDone = true;
                    Serial.println("Final measurement completed");
                }
            }
        }

        // If all measurements complete, export CSV
        if (allMeasurementsComplete) {
            Serial.println("All measurements complete - exporting CSV data");
            printCSVToSerial();

            // Send completion notification via BLE
            if (!finalMeasurementDone) {
                Serial.println("Baseline measurement complete");
                sendBLEStatus("Baseline Complete");
            } else {
                sendBLEStatus("Measurement Complete");
                Serial.println("Final measurement complete");
            }

            allMeasurementsComplete = false;  // Reset flag
        }

        // Small delay to prevent task starvation
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/*=========================SETUP=========================*/
void setup() {
    // Initialize serial for debugging
    Serial.begin(115200);
    delay(1000);  // Give serial time to initialize
    Serial.println("\n\n=== BioPal ESP32-C6 Impedance Analyzer ===");

    // Load calibration data from filesystem
    Serial.println("Loading calibration data...");
    if (loadCalibrationData()) {
        Serial.println("Calibration data loaded successfully");
    } else {
        Serial.println("WARNING: Failed to load calibration data");
    }

    // Create FreeRTOS queue for measurement data
    measurementQueue = xQueueCreate(20, sizeof(MeasurementPoint));
    if (measurementQueue == nullptr) {
        Serial.println("ERROR: Failed to create measurement queue");
        while (1) delay(1000);
    }

    // Initialize UART communication
    initUART(measurementQueue);

    // Initialize BLE communication
    initBLE();
    Serial.println("BLE initialized - ready for WebUI connection");

    // Create FreeRTOS tasks
    xTaskCreate(taskUARTReader, "UART Reader", 4096, nullptr, 2, nullptr);
    xTaskCreate(taskDataProcessor, "Data Processor", 8192, nullptr, 2, nullptr);
    xTaskCreate(taskGUI, "GUI", 4096, nullptr, 1, nullptr);

    Serial.println("All tasks created successfully");
    Serial.println("System ready!\n");
}

/*=========================LOOP=========================*/
// Arduino loop is not used - everything runs in FreeRTOS tasks
void loop() {
    // Empty - FreeRTOS scheduler handles everything
    vTaskDelay(portMAX_DELAY);
}