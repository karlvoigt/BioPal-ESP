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
#include "gui_state.h"
#include "gui_screens.h"
#include "button_handler.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_http_server.h>
#include "ssl_cert.h"

// External declaration for HTTPS server (not properly exposed in Arduino ESP32-C6)
#ifdef __cplusplus
extern "C" {
#endif
typedef struct httpd_ssl_config httpd_ssl_config_t;
struct httpd_ssl_config {
    httpd_config_t httpd;
    const uint8_t *servercert;
    size_t servercert_len;
    const uint8_t *prvtkey_pem;
    size_t prvtkey_len;
    const uint8_t *cacert_pem;
    size_t cacert_len;
    const uint8_t **client_verify_cert_pem;
    size_t *client_verify_cert_len;
    bool use_secure_element;
    void *user_cb;
    bool session_tickets;
    int port_secure;
    int port_insecure;
};
esp_err_t httpd_ssl_start(httpd_handle_t *pHandle, httpd_ssl_config_t *config);
void httpd_ssl_stop(httpd_handle_t handle);

#define HTTPD_SSL_CONFIG_DEFAULT() {   \
        .httpd = HTTPD_DEFAULT_CONFIG(),\
        .servercert = NULL,             \
        .servercert_len = 0,           \
        .prvtkey_pem = NULL,           \
        .prvtkey_len = 0,              \
        .cacert_pem = NULL,            \
        .cacert_len = 0,               \
        .client_verify_cert_pem = NULL,\
        .client_verify_cert_len = NULL,\
        .use_secure_element = false,   \
        .user_cb = NULL,               \
        .session_tickets = false,      \
        .port_secure = 443,            \
        .port_insecure = 80,           \
    }

#ifdef __cplusplus
}
#endif

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
// Splash screen timer (2 seconds)
unsigned long splashStartTime;

// FreeRTOS queue for measurement data
QueueHandle_t measurementQueue;

// WiFi AP and HTTPS Web Server (ESP-IDF native)
httpd_handle_t httpsServer = NULL;

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
            sendBLEError("Invalid Sensor count (must be 1-4)");
            return;
        }

        Serial.printf("[BLE] Starting Baseline measurement with %d Sensor%s...\n", num_duts, num_duts > 1 ? "s" : "");

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
            // Send status update with DUT count
            char statusMsg[32];
            snprintf(statusMsg, sizeof(statusMsg), "Measuring:%d", num_duts);
            sendBLEStatus(statusMsg);
            measurementInProgress = true;

            // Update display GUI to show progress screen
            setGUIState(GUI_BASELINE_PROGRESS);
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
            // Send status update with DUT count
            char statusMsg[32];
            snprintf(statusMsg, sizeof(statusMsg), "Measuring:%d", num_duts);
            sendBLEStatus(statusMsg);
            measurementInProgress = true;

            // Update display GUI to show progress screen
            setGUIState(GUI_FINAL_PROGRESS);
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

        // Return display GUI to home screen
        setGUIState(GUI_HOME);
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

    // Initialize button interrupts
    initButtons();
    Serial.println("Button interrupts initialized");

    // Draw splash screen
    renderCurrentScreen();

    bool splashDone = false;

    Serial.println("\n=== BioPal ESP32 Ready ===");
    Serial.println("Type 'help' for available commands\n");

    // Get semaphore handles
    SemaphoreHandle_t dutCompleteSem = getDUTCompleteSemaphore();
    SemaphoreHandle_t measurementCompleteSem = getMeasurementCompleteSemaphore();
    QueueHandle_t btnEventQueue = getButtonEventQueue();

    bool allMeasurementsComplete = false;

    // Main GUI event loop
    while (true) {
        // Auto-advance from splash screen after 2 seconds
        if (!splashDone && getGUIState() == GUI_SPLASH) {
            if (millis() - splashStartTime >= 2000) {
                setGUIState(GUI_HOME);
                splashDone = true;
            }
        }

        // Process serial commands from computer
        processSerialCommands();

        // Process BLE commands from WebUI
        processBLECommands();

        // Handle button/encoder input
        ButtonEvent event;
        if (xQueueReceive(btnEventQueue, &event, 0) == pdTRUE) {
            handleGUIInput(event);
        }

        // Wait for DUT completion event (10ms timeout for responsive UI)
        if (xSemaphoreTake(dutCompleteSem, pdMS_TO_TICKS(10)) == pdTRUE) {
            // DUT just completed
            uint8_t dutIndex = getCompletedDUTIndex();
            Serial.printf("DUT %d completed\n", dutIndex + 1);

            // Update progress screen
            updateProgressScreen(dutIndex);

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
                    setGUIState(GUI_BASELINE_COMPLETE);
                } else {
                    finalMeasurementDone = true;
                    Serial.println("Final measurement completed");
                    setGUIState(GUI_RESULTS);
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

/*=========================WIFI & WEB SERVER HANDLERS=========================*/
// Helper function to get content type from file extension
const char* getContentType(const String& path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css")) return "text/css";
    if (path.endsWith(".js")) return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".png")) return "image/png";
    if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
    if (path.endsWith(".gif")) return "image/gif";
    if (path.endsWith(".ico")) return "image/x-icon";
    if (path.endsWith(".csv")) return "text/csv";
    return "application/octet-stream";
}

// ESP-IDF handler to serve files from LittleFS
static esp_err_t static_file_handler(httpd_req_t *req) {
    String path = String(req->uri);

    // Default to index.html for root path
    if (path == "/") {
        path = "/index.html";
    }

    // Check if file exists
    if (!LittleFS.exists(path)) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Open file
    fs::File file = LittleFS.open(path, "r");
    if (!file) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Set content type
    httpd_resp_set_type(req, getContentType(path));

    // Read and send file in chunks
    uint8_t buffer[1024];
    while (file.available()) {
        int bytesRead = file.read(buffer, sizeof(buffer));
        if (httpd_resp_send_chunk(req, (const char*)buffer, bytesRead) != ESP_OK) {
            file.close();
            return ESP_FAIL;
        }
    }
    file.close();

    // Signal end of response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/*=========================WIFI & WEB SERVER INIT=========================*/
void initWiFiAndWebServer() {
    Serial.println("Initializing WiFi Access Point...");

    // Configure WiFi AP
    const char* ssid = "BioPal Impedance Analyser";
    const char* password = "";  // Open network (no password)

    // Start WiFi in AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid);

    // Get and display IP address (usually 192.168.4.1 by default)
    IPAddress IP = WiFi.softAPIP();
    Serial.print("WiFi AP Started: ");
    Serial.println(ssid);
    Serial.print("AP IP address: ");
    Serial.println(IP);
    Serial.println("Connect to this WiFi and browse to https://" + IP.toString());

    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("ERROR: LittleFS mount failed");
        return;
    }
    Serial.println("LittleFS mounted successfully");

    // Configure HTTPS server
    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    conf.httpd.server_port = 443;
    conf.httpd.lru_purge_enable = true;

    // Use embedded SSL certificates
    conf.servercert = (const uint8_t*)server_cert;
    conf.servercert_len = strlen(server_cert);
    conf.prvtkey_pem = (const uint8_t*)server_key;
    conf.prvtkey_len = strlen(server_key);

    // Debug output
    Serial.printf("Certificate length: %d bytes\n", conf.servercert_len);
    Serial.printf("Private key length: %d bytes\n", conf.prvtkey_len);
    Serial.printf("Certificate pointer: %p\n", conf.servercert);
    Serial.printf("Private key pointer: %p\n", conf.prvtkey_pem);

    if (conf.servercert_len == 0) {
        Serial.println("ERROR: Certificate is empty!");
        return;
    }
    if (conf.prvtkey_len == 0) {
        Serial.println("ERROR: Private key is empty!");
        return;
    }

    Serial.println("Starting HTTPS server...");

    // Start HTTPS server
    esp_err_t ret = httpd_ssl_start(&httpsServer, &conf);
    if (ret != ESP_OK) {
        Serial.printf("ERROR: Failed to start HTTPS server: %s\n", esp_err_to_name(ret));
        Serial.printf("Error code: 0x%x\n", ret);
        return;
    }

    // Register URI handlers
    // Root handler (serves /index.html)
    httpd_uri_t root_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = static_file_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(httpsServer, &root_uri);

    // Wildcard handler for all other files
    httpd_uri_t wildcard_uri = {
        .uri       = "/*",
        .method    = HTTP_GET,
        .handler   = static_file_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(httpsServer, &wildcard_uri);

    Serial.println("HTTPS Web Server started successfully on port 443");
    Serial.println("Ready to serve WebUI over HTTPS!");
    Serial.println("Web Bluetooth will work with HTTPS secure context");
}

/*=========================SETUP=========================*/
void setup() {
    // Initialize serial for debugging
    vTaskDelay(500);  // Short delay to allow stable startup
    Serial.begin(115200);
    vTaskDelay(500);  // Wait for serial to initialize
    Serial.println("\n\n=== BioPal ESP32-C6 Impedance Analyzer ===");

    // Initialize sprite buffer for flicker-free rendering
    if (!initSpriteBuffer()) {
        Serial.println("WARNING: Sprite buffer failed to initialize - rendering will have flicker");
    }

    // Initialize GUI state machine
    initGUIState();
    splashStartTime = millis();

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

    // Initialize WiFi AP and HTTPS Web Server
    initWiFiAndWebServer();

    // Create FreeRTOS tasks
    xTaskCreate(taskUARTReader, "UART Reader", 4096, nullptr, 2, nullptr);
    xTaskCreate(taskDataProcessor, "Data Processor", 8192, nullptr, 2, nullptr);
    xTaskCreate(taskGUI, "GUI", 4096, nullptr, 1, nullptr);

    Serial.println("All tasks created successfully");
    Serial.println("System ready!\n");
}

/*=========================LOOP=========================*/
// Arduino loop is not used - everything runs in FreeRTOS tasks
// ESP-IDF HTTPS server runs in its own task, no loop() needed
void loop() {
    // Empty - FreeRTOS scheduler handles everything
    vTaskDelay(portMAX_DELAY);
}

// #include <Arduino.h>
// #include <BLEDevice.h>
// #include <BLEUtils.h>
// #include <BLEServer.h>

// // BLE UUIDs - these must match what the web app uses
// #define SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"
// #define CHARACTERISTIC_UUID_RX   "12345678-1234-5678-1234-56789abcdef1"  // WebUI -> ESP32
// #define CHARACTERISTIC_UUID_TX   "12345678-1234-5678-1234-56789abcdef2"  // ESP32 -> WebUI

// // BLE objects
// BLEServer* pServer = nullptr;
// BLECharacteristic* pTxCharacteristic = nullptr;
// BLECharacteristic* pRxCharacteristic = nullptr;

// // Connection state
// bool deviceConnected = false;
// bool oldDeviceConnected = false;

// // Data to send
// uint32_t dataCounter = 0;
// unsigned long lastSendTime = 0;
// const unsigned long SEND_INTERVAL = 1000; // Send data every 1 second

// // LED state
// bool ledState = false;
// const int LED_PIN = 8; // ESP32-C6 built-in LED

// // Server callbacks
// class MyServerCallbacks: public BLEServerCallbacks {
//     void onConnect(BLEServer* pServer) {
//         deviceConnected = true;
//         Serial.println("Client connected");
//     }

//     void onDisconnect(BLEServer* pServer) {
//         deviceConnected = false;
//         Serial.println("Client disconnected");
//     }
// };

// // Characteristic callbacks for receiving commands
// class MyCallbacks: public BLECharacteristicCallbacks {
//     void onWrite(BLECharacteristic* pCharacteristic) {
//         String value = pCharacteristic->getValue().c_str();

//         if (value.length() > 0) {
//             Serial.print("Received command: ");
//             Serial.println(value);

//             // Parse and execute commands
//             if (value == "LED_ON") {
//                 ledState = true;
//                 digitalWrite(LED_PIN, HIGH);
//                 Serial.println("LED turned ON");
//             }
//             else if (value == "LED_OFF") {
//                 ledState = false;
//                 digitalWrite(LED_PIN, LOW);
//                 Serial.println("LED turned OFF");
//             }
//             else if (value == "LED_TOGGLE") {
//                 ledState = !ledState;
//                 digitalWrite(LED_PIN, ledState);
//                 Serial.print("LED toggled to: ");
//                 Serial.println(ledState ? "ON" : "OFF");
//             }
//             else if (value == "GET_STATUS") {
//                 // Send immediate status update
//                 char statusBuffer[50];
//                 snprintf(statusBuffer, sizeof(statusBuffer), "Counter:%lu,LED:%s",
//                          (unsigned long)dataCounter, ledState ? "ON" : "OFF");
//                 pTxCharacteristic->setValue(statusBuffer);
//                 pTxCharacteristic->notify();
//                 Serial.print("Status sent: ");
//                 Serial.println(statusBuffer);
//             }
//             else if (value == "RESET_COUNTER") {
//                 dataCounter = 0;
//                 Serial.println("Counter reset");
//             }
//             else {
//                 Serial.println("Unknown command");
//             }
//         }
//     }
// };

// void setup() {
//     Serial.begin(115200);
//     Serial.println("Starting ESP32-C6 BLE Server...");

//     // Initialize LED
//     pinMode(LED_PIN, OUTPUT);
//     digitalWrite(LED_PIN, LOW);

//     // Create BLE Device
//     BLEDevice::init("ESP32-C6-BioPal");

//     // Create BLE Server
//     pServer = BLEDevice::createServer();
//     pServer->setCallbacks(new MyServerCallbacks());

//     // Create BLE Service
//     BLEService* pService = pServer->createService(SERVICE_UUID);

//     // Create TX Characteristic (for sending data to client)
//     pTxCharacteristic = pService->createCharacteristic(
//         CHARACTERISTIC_UUID_TX,
//         BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
//     );
//     // Note: BLE2902 descriptor is automatically added when NOTIFY property is set

//     // Create RX Characteristic (for receiving commands from client)
//     pRxCharacteristic = pService->createCharacteristic(
//         CHARACTERISTIC_UUID_RX,
//         BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
//     );
//     pRxCharacteristic->setCallbacks(new MyCallbacks());

//     // Start the service
//     pService->start();

//     // Start advertising
//     BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
//     pAdvertising->addServiceUUID(SERVICE_UUID);
//     pAdvertising->setScanResponse(true);
//     pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
//     pAdvertising->setMinPreferred(0x12);
//     BLEDevice::startAdvertising();

//     Serial.println("BLE Server started!");
//     Serial.println("Device name: ESP32-C6-BioPal");
//     Serial.println("Waiting for client connection...");
// }

// void loop() {
//     // Handle connection state changes
//     if (deviceConnected && !oldDeviceConnected) {
//         oldDeviceConnected = deviceConnected;
//         Serial.println("Device connected - ready to send/receive data");
//     }

//     if (!deviceConnected && oldDeviceConnected) {
//         delay(500); // give the bluetooth stack time to get ready
//         pServer->startAdvertising(); // restart advertising
//         Serial.println("Start advertising again");
//         oldDeviceConnected = deviceConnected;
//     }

//     // Send periodic data updates when connected
//     if (deviceConnected) {
//         unsigned long currentTime = millis();

//         if (currentTime - lastSendTime >= SEND_INTERVAL) {
//             lastSendTime = currentTime;
//             dataCounter++;

//             // Create data string with counter and LED status
//             char dataBuffer[50];
//             snprintf(dataBuffer, sizeof(dataBuffer), "Counter:%lu,LED:%s",
//                      (unsigned long)dataCounter, ledState ? "ON" : "OFF");

//             // Send notification to client
//             pTxCharacteristic->setValue(dataBuffer);
//             pTxCharacteristic->notify();

//             Serial.print("Sent data: ");
//             Serial.println(dataBuffer);
//         }
//     }

//     delay(10);
// }
