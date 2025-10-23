#include "BLE_Functions.h"
#include <ArduinoJson.h>

/*=========================GLOBAL BLE OBJECTS=========================*/
static BLEServer* pServer = nullptr;
static BLECharacteristic* pTxCharacteristic = nullptr;
static BLECharacteristic* pRxCharacteristic = nullptr;

// Connection state tracking
static bool deviceConnected = false;
static bool oldDeviceConnected = false;
static bool connectionChanged = false;

// Command buffer
static String receivedCommand = "";
static bool commandReady = false;

/*=========================BLE SERVER CALLBACKS=========================*/
class BioPalServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        connectionChanged = true;
        Serial.println("[BLE] Client connected");
        Serial.printf("[BLE] Connection count: %d\n", pServer->getConnectedCount());
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        connectionChanged = true;
        Serial.println("[BLE] Client disconnected");
        Serial.println("[BLE] Restarting advertising...");

        // Restart advertising after disconnect
        delay(500); // Give bluetooth stack time
        pServer->startAdvertising();
        Serial.println("[BLE] Advertising restarted");
    }
};

/*=========================BLE CHARACTERISTIC CALLBACKS=========================*/
class BioPalCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        String value = pCharacteristic->getValue().c_str();

        if (value.length() > 0) {
            receivedCommand = value;
            commandReady = true;

            Serial.print("[BLE] Received command: '");
            Serial.print(receivedCommand);
            Serial.println("'");
        }
    }
};

/*=========================INITIALIZATION=========================*/
void initBLE() {
    Serial.println("[BLE] Initializing BLE...");

    // Create BLE device
    BLEDevice::init(BLE_DEVICE_NAME);
    Serial.printf("[BLE] Device name: %s\n", BLE_DEVICE_NAME);

    // Create BLE server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new BioPalServerCallbacks());
    Serial.println("[BLE] Server created");

    // Create BLE service
    BLEService* pService = pServer->createService(BLE_SERVICE_UUID);
    Serial.printf("[BLE] Service UUID: %s\n", BLE_SERVICE_UUID);

    // Create TX characteristic (ESP32 -> WebUI)
    pTxCharacteristic = pService->createCharacteristic(
        BLE_CHARACTERISTIC_TX,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    // Add descriptor for notifications
    pTxCharacteristic->addDescriptor(new BLE2902());
    Serial.println("[BLE] TX characteristic created (for sending data to WebUI)");

    // Create RX characteristic (WebUI -> ESP32)
    pRxCharacteristic = pService->createCharacteristic(
        BLE_CHARACTERISTIC_RX,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new BioPalCharacteristicCallbacks());
    Serial.println("[BLE] RX characteristic created (for receiving commands from WebUI)");

    // Start the service
    pService->start();
    Serial.println("[BLE] Service started");

    // Start advertising
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x0006);  // 7.5ms
    pAdvertising->setMaxPreferred(0x0012);  // 22.5ms
    BLEDevice::startAdvertising();

    Serial.println("[BLE] ========================================");
    Serial.println("[BLE] BLE Server started successfully!");
    Serial.printf("[BLE] Device Name: %s\n", BLE_DEVICE_NAME);
    Serial.println("[BLE] Waiting for client connection...");
    Serial.println("[BLE] ========================================");
}

/*=========================CONNECTION STATUS=========================*/
bool isBLEConnected() {
    return deviceConnected;
}

bool getBLEConnectionChanged() {
    return connectionChanged;
}

void clearBLEConnectionChanged() {
    connectionChanged = false;
    oldDeviceConnected = deviceConnected;
}

/*=========================COMMAND PROCESSING=========================*/
bool getBLECommand(char* cmdBuffer, size_t maxLen) {
    if (!commandReady) {
        return false;
    }

    // Copy command to buffer
    strncpy(cmdBuffer, receivedCommand.c_str(), maxLen - 1);
    cmdBuffer[maxLen - 1] = '\0';  // Ensure null termination

    // Clear command flag
    commandReady = false;
    receivedCommand = "";

    return true;
}

uint8_t parseStartCommand(const char* cmd) {
    // Expected format: "START:n" where n is 1-4
    String cmdStr(cmd);

    if (!cmdStr.startsWith(BLE_CMD_START)) {
        Serial.println("[BLE] ERROR: Not a START command");
        return 0;
    }

    int colonIndex = cmdStr.indexOf(':');
    if (colonIndex < 0) {
        Serial.println("[BLE] WARNING: START command without DUT count, using default 4");
        return 4;  // Default to 4 DUTs
    }

    String numStr = cmdStr.substring(colonIndex + 1);
    numStr.trim();
    int num_duts = numStr.toInt();

    if (num_duts < 1 || num_duts > 4) {
        Serial.printf("[BLE] ERROR: Invalid DUT count %d (must be 1-4)\n", num_duts);
        return 0;
    }

    Serial.printf("[BLE] Parsed START command: %d DUT%s\n", num_duts, num_duts > 1 ? "s" : "");
    return (uint8_t)num_duts;
}

/*=========================DATA TRANSMISSION=========================*/
bool sendBLEString(const char* data) {
    if (!deviceConnected || !pTxCharacteristic) {
        Serial.println("[BLE] WARNING: Cannot send - no client connected");
        return false;
    }

    size_t len = strlen(data);
    if (len == 0) {
        Serial.println("[BLE] WARNING: Attempted to send empty string");
        return false;
    }

    // BLE has MTU limit (typically 23-512 bytes depending on negotiation)
    // For large data, we might need to chunk it, but for now send as-is
    pTxCharacteristic->setValue((uint8_t*)data, len);
    pTxCharacteristic->notify();

    Serial.printf("[BLE] Sent (%d bytes): %s\n", len, data);
    return true;
}

void sendBLEStatus(const char* status) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s:%s", BLE_RESP_STATUS, status);
    sendBLEString(buffer);
}

void sendBLEDUTStart(uint8_t dutNum) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%s:%d", BLE_RESP_DUT_START, dutNum);
    sendBLEString(buffer);
}

void sendBLEDUTEnd(uint8_t dutNum) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%s:%d", BLE_RESP_DUT_END, dutNum);
    sendBLEString(buffer);
}

bool sendBLEImpedanceData(uint8_t dutIndex) {
    if (dutIndex >= MAX_DUT_COUNT) {
        Serial.printf("[BLE] ERROR: Invalid DUT index %d\n", dutIndex);
        return false;
    }

    if (frequencyCount[dutIndex] == 0) {
        Serial.printf("[BLE] WARNING: No data for DUT %d\n", dutIndex + 1);
        return false;
    }

    Serial.printf("[BLE] Preparing to send data for DUT %d (%d points)...\n",
                  dutIndex + 1, frequencyCount[dutIndex]);

    // Create JSON document
    // Size calculation: ~50 bytes overhead + ~60 bytes per point
    size_t jsonSize = 200 + (frequencyCount[dutIndex] * 80);
    JsonDocument doc;

    // Add DUT number
    doc["dut"] = dutIndex + 1;
    doc["count"] = frequencyCount[dutIndex];

    // Create arrays for frequency, magnitude, and phase
    JsonArray freqArray = doc["freq"].to<JsonArray>();
    JsonArray magArray = doc["mag"].to<JsonArray>();
    JsonArray phaseArray = doc["phase"].to<JsonArray>();

    // Fill arrays with impedance data
    for (int i = 0; i < frequencyCount[dutIndex]; i++) {
        ImpedancePoint& point = impedanceData[dutIndex][i];

        if (point.valid) {
            freqArray.add(point.freq_hz);
            magArray.add(serialized(String(point.Z_magnitude, 6)));  // 6 decimal places
            phaseArray.add(serialized(String(point.Z_phase, 2)));     // 2 decimal places
        }
    }

    // Serialize to string
    String jsonStr;
    serializeJson(doc, jsonStr);

    Serial.printf("[BLE] JSON size: %d bytes\n", jsonStr.length());
    Serial.println("[BLE] JSON preview (first 200 chars):");
    Serial.println(jsonStr.substring(0, min(200, (int)jsonStr.length())));

    // Send with DATA prefix
    String dataMsg = String(BLE_RESP_DATA) + ":" + jsonStr;

    // Check if data fits in single BLE packet (conservative limit)
    if (dataMsg.length() > 512) {
        Serial.println("[BLE] WARNING: Data might exceed BLE MTU - consider chunking");
        // For now, try to send anyway - BLE stack may handle it
    }

    bool success = sendBLEString(dataMsg.c_str());

    if (success) {
        Serial.printf("[BLE] Successfully sent impedance data for DUT %d\n", dutIndex + 1);
    } else {
        Serial.printf("[BLE] FAILED to send impedance data for DUT %d\n", dutIndex + 1);
    }

    return success;
}

void sendBLEComplete() {
    sendBLEString(BLE_RESP_COMPLETE);
    Serial.println("[BLE] Sent measurement complete notification");
}

void sendBLEError(const char* errorMsg) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s:%s", BLE_RESP_ERROR, errorMsg);
    sendBLEString(buffer);
}

/*=========================UTILITY=========================*/
void enableBLE(bool enable) {
    if (enable) {
        Serial.println("[BLE] Enabling BLE advertising...");
        pServer->startAdvertising();
    } else {
        Serial.println("[BLE] Disabling BLE advertising...");
        pServer->getAdvertising()->stop();
    }
}
