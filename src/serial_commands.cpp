#include "serial_commands.h"
#include "UART_Functions.h"
#include "defines.h"
#include <string.h>

#define CMD_BUFFER_SIZE 64

void processSerialCommands() {
    // Check if data available on USB serial
    if (!Serial.available()) {
        return;
    }

    // Read line from serial
    String cmdLine = Serial.readStringUntil('\n');
    cmdLine.trim();  // Remove whitespace

    if (cmdLine.length() == 0) {
        return;
    }

    Serial.printf("Received command: '%s'\n", cmdLine.c_str());

    // Parse command
    if (cmdLine.startsWith("start")) {
        // Extract number of DUTs if provided
        int num_duts = 4;  // Default

        // Check if there's a number after "start"
        int spaceIndex = cmdLine.indexOf(' ');
        if (spaceIndex > 0) {
            String numStr = cmdLine.substring(spaceIndex + 1);
            num_duts = numStr.toInt();

            // Validate range
            if (num_duts < 1 || num_duts > 4) {
                Serial.printf("ERROR: Invalid number of DUTs (%d). Must be 1-4.\n", num_duts);
                return;
            }
        }

        Serial.printf("Starting measurement with %d DUT%s...\n", num_duts, num_duts > 1 ? "s" : "");

        // Clear previous measurement data
        Serial.println("Clearing measurement buffers...");
        for (int i = 0; i < MAX_DUT_COUNT; i++) {
            frequencyCount[i] = 0;
            // Optionally clear impedance data array
            for (int j = 0; j < MAX_FREQUENCIES; j++) {
                if (baselineMeasurementDone) {
                    measurementImpedanceData[i][j] = ImpedancePoint();
                } else {
                    baselineImpedanceData[i][j] = ImpedancePoint();
                }
            }
        }
        Serial.println("Buffers cleared - ready for new measurement");

        sendStartCommand(num_duts);
    }
    else if (cmdLine.equals("stop")) {
        Serial.println("Stopping measurement...");
        sendStopCommand();
    }
    else if (cmdLine.equals("help")) {
        Serial.println("\n=== Available Commands ===");
        Serial.println("start [num_duts]  - Start measurement (default 4 DUTs, or specify 1-4)");
        Serial.println("stop              - Stop measurement");
        Serial.println("help              - Show this help message");
        Serial.println("========================\n");
    }
    else {
        Serial.printf("ERROR: Unknown command '%s'. Type 'help' for available commands.\n", cmdLine.c_str());
    }
}
