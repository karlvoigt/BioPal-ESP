#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "defines.h"
#include "UART_Functions.h"
#include "calibration.h"
#include "impedance_calc.h"

/*=========================GLOBAL VARIABLES=========================*/
// Initialize global impedance data arrays
ImpedancePoint impedanceData[MAX_DUT_COUNT][MAX_FREQUENCIES];
int frequencyCount[MAX_DUT_COUNT] = {0};

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

            // Calibrate the measurement point
            if (calibrate(point)) {
                Serial.printf("Calibrated: V=%.6fV, I=%.6fmA, Phase=%.2f\n",
                             point.V_magnitude, point.I_magnitude, point.phase_deg);
            } else {
                Serial.printf("WARNING: Calibration failed for freq=%lu Hz\n", point.freq_hz);
            }

            // Calculate impedance: Z = V / I
            ImpedancePoint impedance = calcImpedance(point);

            // Store in global impedance array
            int freqIndex = frequencyCount[dutIndex];
            if (freqIndex < MAX_FREQUENCIES) {
                impedanceData[dutIndex][freqIndex] = impedance;
                frequencyCount[dutIndex]++;
            } else {
                Serial.printf("ERROR: Frequency buffer full for DUT %d\n", dutIndex + 1);
            }
        }
    }
}

/*=========================TASK: GUI=========================*/
// Task to handle GUI and user interaction
void taskGUI(void* parameter) {
    Serial.println("GUI task started");

    // Wait a bit for system to stabilize
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Send start command to STM32
    sendStartCommand();
    Serial.println("Start command sent to STM32");

    // Main GUI loop
    while (true) {
        // TODO: Implement TFT display updates and button handling
        // For now, just delay
        vTaskDelay(pdMS_TO_TICKS(1000));
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