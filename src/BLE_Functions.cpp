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

extern void drawConnectionIndicatorDefault(bool connected);


/*=========================BLE SERVER CALLBACKS=========================*/
class BioPalServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        connectionChanged = true;
        Serial.println("[BLE] Client connected");
        Serial.printf("[BLE] Connection count: %d\n", pServer->getConnectedCount());
        // drawConnectionIndicatorDefault(true);
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        connectionChanged = true;
        Serial.println("[BLE] Client disconnected");
        Serial.println("[BLE] Restarting advertising...");
        drawConnectionIndicatorDefault(false);
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
    // Set mtu size before init to allow larger packe
    BLEDevice::init(BLE_DEVICE_NAME);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_N12);
    Serial.printf("[BLE] Device name: %s\n", BLE_DEVICE_NAME);

    // Create BLE server
    BLEDevice::setMTU(517);
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new BioPalServerCallbacks());

    // Set MTU size for larger packets
    Serial.println("[BLE] Server created with MTU=517");

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

    // Allow BLE stack to stabilize before advertising
    Serial.println("[BLE] BLE stack stabilized");

    // Start advertising - PROPERLY SPLIT DATA TO AVOID 31-BYTE OVERFLOW
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();

    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    // Set advertising interval for fast discovery (20-40ms)
    pAdvertising->setMinInterval(0x20);  // 20ms (0x20 * 0.625ms)
    pAdvertising->setMaxInterval(0x40);  // 40ms (0x40 * 0.625ms)

    BLEDevice::startAdvertising();

    Serial.println("[BLE] ========================================");
    Serial.println("[BLE] BLE Server started successfully!");
    Serial.printf("[BLE] Device Name: %s\n", BLE_DEVICE_NAME);
    Serial.println("[BLE] Waiting for client connection...");
    Serial.println("[BLE] ========================================");
}

/*=========================BLE RESET=========================*/
void resetBLE() {
    Serial.println("[BLE] Manual BLE reset requested");
    Serial.println("[BLE] Deinitializing BLE stack...");

    BLEDevice::deinit(true);  // Complete teardown
    delay(1000);  // Allow full shutdown

    Serial.println("[BLE] Reinitializing BLE...");
    initBLE();

    Serial.println("[BLE] BLE reset complete");
}

/*=========================CONNECTION STATUS=========================*/
bool isBLEConnected() {
    return deviceConnected;
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

void parseStartCommand(const char* cmd, uint8_t& num_duts, uint8_t& start_idx, uint8_t& stop_idx) {
    // Expect format:
    // 1) With commas: "START:n,SS,EE"  (SS and EE are exactly 2 chars, may have leading zeros)
    // 2) Without commas: "START:nSSEE" (n = 1 char, SS = 2 chars, EE = 2 chars)
    String cmdStr(cmd);

    // defaults
    num_duts = 4;
    start_idx = 0;
    stop_idx = 0;

    if (!cmdStr.startsWith(BLE_CMD_BASELINE)) {
        Serial.println("[BLE] ERROR: Not a START command");
        num_duts = 0;
        return;
    }

    int colonIndex = cmdStr.indexOf(':');
    if (colonIndex < 0) {
        Serial.println("[BLE] WARNING: START command without parameters, using defaults (4,0,0)");
        return;
    }

    String params = cmdStr.substring(colonIndex + 1);
    params.trim();
    params.replace(",", "");
    if (params.length() != 5) {
        Serial.println("[BLE] WARNING: START command has empty parameters, using defaults (4,0,0)");
        return;
    }

    int n = params.charAt(0) - '0';
    if (n < 1 || n > 4) {
        Serial.printf("[BLE] ERROR: Invalid DUT count %d (must be 1-4)\n", n);
        num_duts = 0;
        return;
    }
    num_duts = (uint8_t)n;

    String sStart = params.substring(1, 3);
    start_idx = (uint8_t)sStart.toInt();
    
    
    String sStop = params.substring(3, 5);
    stop_idx = (uint8_t)sStop.toInt();

    Serial.printf("[BLE] Parsed START command: %d DUT%s, start=%u, stop=%u\n",
                  num_duts, num_duts > 1 ? "s" : "", start_idx, stop_idx);
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

    // BLE MTU limit - use conservative chunk size
    const size_t MAX_CHUNK_SIZE = 400;

    if (len <= MAX_CHUNK_SIZE) {
        // Small enough to send in one packet
        pTxCharacteristic->setValue((uint8_t*)data, len);
        pTxCharacteristic->notify();
        Serial.printf("[BLE] Sent (%d bytes): %s\n", len, data);
        return true;
    }

    // Need to chunk the data
    Serial.printf("[BLE] Data too large (%d bytes), chunking...\n", len);

    size_t offset = 0;
    int chunkNum = 0;

    while (offset < len) {
        size_t chunkSize = min(MAX_CHUNK_SIZE, len - offset);

        // Send chunk
        pTxCharacteristic->setValue((uint8_t*)(data + offset), chunkSize);
        pTxCharacteristic->notify();

        Serial.printf("[BLE] Sent chunk %d (%d bytes)\n", chunkNum, chunkSize);

        offset += chunkSize;
        chunkNum++;

        // Small delay between chunks to avoid overwhelming receiver
        delay(20);
    }

    Serial.printf("[BLE] Sent %d chunks (total %d bytes)\n", chunkNum, len);
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
        if (!baselineMeasurementDone) {
            ImpedancePoint& point = baselineImpedanceData[dutIndex][i];

            if (point.valid) {
                freqArray.add(point.freq_hz);
                magArray.add(serialized(String(point.Z_magnitude, 3)));  // 3 decimal places (reduced for smaller JSON)
                phaseArray.add(serialized(String(point.Z_phase, 2)));     // 2 decimal places
            }
        } else {
            ImpedancePoint& point = measurementImpedanceData[dutIndex][i];

            if (point.valid) {
                freqArray.add(point.freq_hz);
                magArray.add(serialized(String(point.Z_magnitude, 3)));  // 3 decimal places (reduced for smaller JSON)
                phaseArray.add(serialized(String(point.Z_phase, 2)));     // 2 decimal places
            }
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
