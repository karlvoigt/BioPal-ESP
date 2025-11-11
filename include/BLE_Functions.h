#ifndef BLE_FUNCTIONS_H
#define BLE_FUNCTIONS_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "defines.h"

// BLE UUIDs for BioPal service
#define BLE_SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"
#define BLE_CHARACTERISTIC_RX   "12345678-1234-5678-1234-56789abcdef1"  // WebUI -> ESP32
#define BLE_CHARACTERISTIC_TX   "12345678-1234-5678-1234-56789abcdef2"  // ESP32 -> WebUI

// BLE device name
#define BLE_DEVICE_NAME "BioPal"

// BLE command types
#define BLE_CMD_BASELINE   "BASELINE_START"
#define BLE_CMD_STOP    "STOP"
#define BLE_CMD_MEAS    "MEAS_START"

// BLE response types
#define BLE_RESP_STATUS     "STATUS"
#define BLE_RESP_DUT_START  "DUT_START"
#define BLE_RESP_DATA       "DATA"
#define BLE_RESP_DUT_END    "DUT_END"
#define BLE_RESP_COMPLETE   "COMPLETE"
#define BLE_RESP_ERROR      "ERROR"

/*=========================BLE INITIALIZATION=========================*/
// Initialize BLE server and characteristics
// Sets up callbacks for connection and command reception
void initBLE();

// Reset BLE stack completely (deinit + reinit)
// Use this if BLE gets into stuck state
void resetBLE();

/*=========================BLE CONNECTION STATUS=========================*/
// Check if a BLE client is currently connected
bool isBLEConnected();

// Get connection state change flag (for detecting new connections)
bool getBLEConnectionChanged();

// Clear connection state change flag
void clearBLEConnectionChanged();

/*=========================BLE COMMAND PROCESSING=========================*/
// Check if new command received and get it
// Returns true if command available, false otherwise
// Command string will be copied to cmdBuffer (null-terminated)
bool getBLECommand(char* cmdBuffer, size_t maxLen);

// Parse START command to extract number of DUTs, start index, and stop index
void parseStartCommand(const char* cmd, uint8_t& num_duts, uint8_t& start_idx, uint8_t& stop_idx, float &calcStartFreq, float &calcEndFreq);

/*=========================BLE DATA TRANSMISSION=========================*/
// Send status message to WebUI
// Examples: "STATUS:Ready", "STATUS:Measuring"
void sendBLEStatus(const char* status);

// Send DUT start notification
// dutNum: 1-4
void sendBLEDUTStart(uint8_t dutNum);

// Send DUT end notification
// dutNum: 1-4
void sendBLEDUTEnd(uint8_t dutNum);

// Send impedance data for a DUT as JSON
// dutIndex: 0-3 (for DUT 1-4)
// Returns true if sent successfully
bool sendBLEImpedanceData(uint8_t dutIndex);

// Send measurement complete notification
void sendBLEComplete();

// Send error message
void sendBLEError(const char* errorMsg);

/*=========================BLE UTILITY=========================*/
// Send raw string over BLE TX characteristic
// Returns true if sent successfully
bool sendBLEString(const char* data);

// Enable/disable BLE (for power saving)
void enableBLE(bool enable);

#endif // BLE_FUNCTIONS_H
