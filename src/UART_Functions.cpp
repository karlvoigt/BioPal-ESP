#include "UART_Functions.h"

// UART serial instance
HardwareSerial UARTSerial(1);  // Use Serial1 for ESP32-C6

// Queue handle for sending measurement points to processing task
static QueueHandle_t measurementQueueHandle = nullptr;

// Semaphore for signaling when data is received
static SemaphoreHandle_t uartDataSemaphore = nullptr;

// Semaphores for event signaling to GUI task
static SemaphoreHandle_t dutCompleteSemaphore = nullptr;
static SemaphoreHandle_t measurementCompleteSemaphore = nullptr;

// DUT completion tracking
static uint8_t completedDUTIndex = 0;
static uint8_t totalExpectedDUTs = 4;  // Default to 4, updated on START command
static uint8_t completedDUTCount = 0;

// Circular buffer for ISR byte collection
#define CIRC_BUFFER_SIZE 512
static uint8_t circBuffer[CIRC_BUFFER_SIZE];
static volatile uint16_t circBufferHead = 0;
static volatile uint16_t circBufferTail = 0;

// Receiver context (used by processing task, not ISR)
static UARTRxContext rxContext;

// ACK reception tracking
static volatile bool ackReceived = false;
static volatile uint8_t ackCmdType = 0;

/*=========================CIRCULAR BUFFER HELPERS=========================*/

// Check if buffer has data available
static inline bool circBufferAvailable() {
    return circBufferHead != circBufferTail;
}

// Read one byte from circular buffer (call from task, not ISR)
static uint8_t circBufferRead() {
    uint8_t byte = circBuffer[circBufferTail];
    circBufferTail = (circBufferTail + 1) % CIRC_BUFFER_SIZE;
    return byte;
}

// Write one byte to circular buffer (call from ISR)
static inline void circBufferWrite(uint8_t byte) {
    uint16_t nextHead = (circBufferHead + 1) % CIRC_BUFFER_SIZE;
    if (nextHead != circBufferTail) {  // Check for overflow
        circBuffer[circBufferHead] = byte;
        circBufferHead = nextHead;
    }
    // If buffer full, oldest data is dropped (could add error counter here)
}

/*=========================INTERRUPT CALLBACK=========================*/

// UART receive interrupt callback - called when data arrives
// MINIMAL ISR - only buffers bytes and signals task
void IRAM_ATTR onUARTReceive() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Copy all available bytes to circular buffer (fast operation)
    while (UARTSerial.available()) {
        uint8_t byte = UARTSerial.read();
        circBufferWrite(byte);
    }

    // Signal semaphore to wake up processing task
    if (uartDataSemaphore != nullptr) {
        xSemaphoreGiveFromISR(uartDataSemaphore, &xHigherPriorityTaskWoken);
    }

    // Yield if higher priority task was woken
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/*=========================INITIALIZATION=========================*/

void initUART(QueueHandle_t measurementQueue) {
    measurementQueueHandle = measurementQueue;

    // Create semaphores for signaling
    uartDataSemaphore = xSemaphoreCreateBinary();
    dutCompleteSemaphore = xSemaphoreCreateBinary();
    measurementCompleteSemaphore = xSemaphoreCreateBinary();

    // Initialize UART with 3600 baud on pins 2 (RX) and 3 (TX)
    UARTSerial.begin(UART_BAUD_RATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

    // Attach interrupt callback for received data
    UARTSerial.onReceive(onUARTReceive);

    // Initialize receiver state machine
    rxContext.state = WAITING_START;
    rxContext.byteCount = 0;
    rxContext.expectedBytes = 0;
    rxContext.packetType = 0;
    rxContext.currentDUT = 0;
    rxContext.expectedFreqCount = 0;

    Serial.printf("UART initialized: RX=GPIO%d, TX=GPIO%d, Baud=%d\n",
                  UART_RX_PIN, UART_TX_PIN, UART_BAUD_RATE);
    Serial.println("Interrupt-driven reception enabled (Arduino onReceive)");
}

SemaphoreHandle_t getUARTSemaphore() {
    return uartDataSemaphore;
}

// Process all buffered bytes from circular buffer
// This runs in task context with full stack - safe for heavy processing
void processBufferedBytes() {
    while (circBufferAvailable()) {
        uint8_t byte = circBufferRead();
        processIncomingByte(byte);
    }
}

/*=========================COMMAND SENDING=========================*/

bool sendCommand(uint8_t cmd_type, uint32_t data1, uint32_t data2, uint32_t data3) {
    uint8_t packet[UART_CMD_PACKET_SIZE];

    // Build command packet (little-endian)
    packet[0] = UART_CMD_START_BYTE;
    packet[1] = cmd_type;

    // data1 (4 bytes)
    packet[2] = (data1 >> 0) & 0xFF;
    packet[3] = (data1 >> 8) & 0xFF;
    packet[4] = (data1 >> 16) & 0xFF;
    packet[5] = (data1 >> 24) & 0xFF;

    // data2 (4 bytes)
    packet[6] = (data2 >> 0) & 0xFF;
    packet[7] = (data2 >> 8) & 0xFF;
    packet[8] = (data2 >> 16) & 0xFF;
    packet[9] = (data2 >> 24) & 0xFF;

    // data3 (4 bytes)
    packet[10] = (data3 >> 0) & 0xFF;
    packet[11] = (data3 >> 8) & 0xFF;
    packet[12] = (data3 >> 16) & 0xFF;
    packet[13] = (data3 >> 24) & 0xFF;

    packet[14] = UART_CMD_END_BYTE;

    // Send packet
    UARTSerial.write(packet, UART_CMD_PACKET_SIZE);
    UARTSerial.flush();
    return true;
}

bool sendStartCommand() {
    Serial.println("Sending START command to STM32 (4 DUTs)");
    return sendStartCommand(4);
}

bool sendStartCommand(uint8_t num_duts, uint8_t startIDX, uint8_t endIDX) {
    Serial.printf("Sending START command to STM32 (%d DUT%s)\n", num_duts, num_duts > 1 ? "s" : "");
    totalExpectedDUTs = num_duts;
    completedDUTCount = 0;  // Reset counter

    // Retry up to 3 times if no ACK
    for (int attempt = 0; attempt < 3; attempt++) {
        sendCommand(CMD_START_MEASUREMENT, num_duts, startIDX, endIDX);

        if (waitForAck(CMD_START_MEASUREMENT, 1000)) {
            Serial.println("START command acknowledged");
            return true;  // Success
        }

        Serial.printf("Retry %d/3...\n", attempt + 1);
        delay(100);  // Wait before retry
    }

    Serial.println("ERROR: START command failed after 3 attempts");
    return false;
}

bool sendStopCommand() {
    Serial.println("Sending STOP command to STM32");

    // Retry up to 3 times if no ACK
    for (int attempt = 0; attempt < 3; attempt++) {
        sendCommand(CMD_END_MEASUREMENT, 0, 0, 0);

        if (waitForAck(CMD_END_MEASUREMENT, 1000)) {
            Serial.println("STOP command acknowledged");
            return true;  // Success
        }

        Serial.printf("Retry %d/3...\n", attempt + 1);
        delay(100);  // Wait before retry
    }

    Serial.println("ERROR: STOP command failed after 3 attempts");
    return false;
}

bool sendSetPGAGainCommand(uint8_t gain) {
    Serial.printf("Sending SET_PGA_GAIN command: %d\n", gain);
    sendCommand(CMD_SET_PGA_GAIN, gain, 0, 0);
    return true;
}

bool sendSetMuxChannelCommand(uint8_t channel) {
    Serial.printf("Sending SET_MUX_CHANNEL command: %d\n", channel);
    sendCommand(CMD_SET_MUX_CHANNEL, channel, 0, 0);
    return true;
}

bool sendSetTIAGainCommand(uint8_t low_gain) {
    Serial.printf("Sending SET_TIA_GAIN command: %s\n", low_gain ? "LOW" : "HIGH");
    sendCommand(CMD_SET_TIA_GAIN, low_gain, 0, 0);
    return true;
}

/*=========================HELPER FUNCTIONS=========================*/

// Convert 4 bytes to uint32 (little-endian)
static uint32_t bytesToUint32(uint8_t* bytes) {
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

// Convert 4 bytes to int32 (little-endian)
static int32_t bytesToInt32(uint8_t* bytes) {
    return (int32_t)bytesToUint32(bytes);
}

// Convert 4 bytes to float (IEEE 754)
static float bytesToFloat(uint8_t* bytes) {
    float result;
    memcpy(&result, bytes, sizeof(float));
    return result;
}

// Parse and queue a frequency packet
static void parseFrequencyPacket() {
    MeasurementPoint point;

    // Parse frequency (4 bytes at index 2)
    point.freq_hz = bytesToUint32(&rxContext.buffer[2]);

    // Parse voltage magnitude (4 bytes at index 6) - float
    point.V_magnitude = bytesToFloat(&rxContext.buffer[6]);

    // Parse voltage phase (4 bytes at index 10) - float
    float v_phase = bytesToFloat(&rxContext.buffer[10]);

    // Parse current magnitude (4 bytes at index 14) - float
    point.I_magnitude = bytesToFloat(&rxContext.buffer[14]);

    // Parse current phase (4 bytes at index 18) - float
    float i_phase = bytesToFloat(&rxContext.buffer[18]);

    // Calculate phase difference (V - I) and normalize to [-180, 180]
    float phase_diff = v_phase - i_phase;
    while (phase_diff > 180.0f) phase_diff -= 360.0f;
    while (phase_diff < -180.0f) phase_diff += 360.0f;
    point.phase_deg = phase_diff;

    // Parse PGA gain (1 byte at index 22)
    point.pga_gain = rxContext.buffer[22];

    // Parse TIA gain (1 byte at index 23)
    point.tia_gain = (rxContext.buffer[23] == 1);  // 1=high, 0=low

    // Parse valid flag (1 byte at index 24)
    point.valid = (rxContext.buffer[24] == 1);

    // Send to queue
    if (measurementQueueHandle != nullptr) {
        if (xQueueSend(measurementQueueHandle, &point, pdMS_TO_TICKS(100)) != pdTRUE) {
            Serial.println("ERROR: Failed to queue measurement point!");
        } else {
            Serial.printf("Queued: DUT%d Freq=%lu Hz, V=%.3f, I=%.3f, Phase=%.2f°, Valid=%d\n",
                         rxContext.currentDUT, point.freq_hz, point.V_magnitude,
                         point.I_magnitude, point.phase_deg, point.valid);
        }
    }
}

/*=========================PACKET RECEIVING=========================*/

void processIncomingByte(uint8_t byte) {
    switch (rxContext.state) {
        case WAITING_START:
            if (byte == UART_DATA_START_BYTE) {
                rxContext.buffer[0] = byte;
                rxContext.byteCount = 1;
                rxContext.state = READING_PACKET_TYPE;
            }
            break;

        case READING_PACKET_TYPE:
            rxContext.buffer[1] = byte;
            rxContext.packetType = byte;
            rxContext.byteCount = 2;

            // Check if this might be an ACK packet (cmd_type 0x01-0x05)
            if (byte >= CMD_SET_PGA_GAIN && byte <= CMD_SET_TIA_GAIN) {
                // Could be ACK packet - need to read next 2 bytes to confirm
                rxContext.expectedBytes = UART_ACK_PACKET_SIZE;
                rxContext.state = READING_DUT_START;  // Reuse state for ACK reading
            }
            // Determine data packet type and expected size
            else if (byte == UART_DATA_DUT_START) {
                rxContext.expectedBytes = UART_DATA_DUT_START_SIZE;
                rxContext.state = READING_DUT_START;
            } else if (byte == UART_DATA_FREQUENCY) {
                rxContext.expectedBytes = UART_DATA_FREQUENCY_SIZE;
                rxContext.state = READING_FREQUENCY;
            } else if (byte == UART_DATA_DUT_END) {
                rxContext.expectedBytes = UART_DATA_DUT_END_SIZE;
                rxContext.state = READING_DUT_END;
            } else {
                // Unknown packet type, reset
                Serial.printf("Unknown packet type: 0x%02X\n", byte);
                rxContext.state = WAITING_START;
            }
            break;

        case READING_DUT_START:
        case READING_FREQUENCY:
        case READING_DUT_END:
            // Collect bytes for current packet type
            // All three packet types use the same collection logic
            rxContext.buffer[rxContext.byteCount++] = byte;

            // Check if we've received all expected bytes for this packet
            if (rxContext.byteCount >= rxContext.expectedBytes) {
                // Validate end byte
                if (rxContext.buffer[rxContext.byteCount - 1] == UART_DATA_END_BYTE) {
                    // Check if this is an ACK packet (4 bytes, third byte is 0x01, packet type is a command)
                    if (rxContext.expectedBytes == UART_ACK_PACKET_SIZE &&
                        rxContext.buffer[2] == 0x01 &&
                        rxContext.packetType >= CMD_SET_PGA_GAIN &&
                        rxContext.packetType <= CMD_SET_TIA_GAIN) {
                        // ACK packet received
                        ackCmdType = rxContext.packetType;
                        ackReceived = true;
                        Serial.printf("ACK received for command 0x%02X\n", ackCmdType);
                    }
                    // Process data packets
                    else if (rxContext.packetType == UART_DATA_DUT_START) {
                        rxContext.currentDUT = rxContext.buffer[2];
                        rxContext.expectedFreqCount = rxContext.buffer[3];
                        Serial.printf("\n=== DUT %d START (expecting %d frequencies) ===\n",
                                     rxContext.currentDUT, rxContext.expectedFreqCount);
                    }
                    else if (rxContext.packetType == UART_DATA_FREQUENCY) {
                        parseFrequencyPacket();
                    }
                    else if (rxContext.packetType == UART_DATA_DUT_END) {
                        uint8_t dutNum = rxContext.buffer[2];
                        Serial.printf("=== DUT %d END ===\n\n", dutNum);

                        // Signal GUI task that DUT is complete
                        completedDUTIndex = dutNum - 1;  // Convert 1-4 to 0-3
                        completedDUTCount++;

                        if (dutCompleteSemaphore != nullptr) {
                            xSemaphoreGive(dutCompleteSemaphore);
                        }

                        // Check if all DUTs complete
                        if (completedDUTCount >= totalExpectedDUTs) {
                            Serial.println("=== ALL MEASUREMENTS COMPLETE ===");
                            if (measurementCompleteSemaphore != nullptr) {
                                xSemaphoreGive(measurementCompleteSemaphore);
                            }
                        }
                    }
                } else {
                    Serial.printf("Invalid end byte: 0x%02X\n",
                                 rxContext.buffer[rxContext.byteCount - 1]);
                }

                // Reset state machine
                rxContext.state = WAITING_START;
                rxContext.byteCount = 0;
            }
            break;
    }
}

uint8_t getCurrentDUT() {
    return rxContext.currentDUT;
}

/*=========================ACK HANDLING=========================*/

bool waitForAck(uint8_t cmd_type, uint32_t timeout_ms) {
    // Reset ACK flags
    ackReceived = false;
    ackCmdType = 0;

    uint32_t startTime = millis();

    // Wait for ACK or timeout
    while (millis() - startTime < timeout_ms) {
        if (ackReceived && ackCmdType == cmd_type) {
            ackReceived = false;  // Reset flag
            return true;
        }
        delay(1);  // Small delay to prevent tight loop
    }

    // Timeout - no ACK received
    Serial.printf("WARNING: No ACK received for command 0x%02X\n", cmd_type);
    return false;
}

/*=========================EVENT SIGNALING=========================*/

SemaphoreHandle_t getDUTCompleteSemaphore() {
    return dutCompleteSemaphore;
}

SemaphoreHandle_t getMeasurementCompleteSemaphore() {
    return measurementCompleteSemaphore;
}

uint8_t getCompletedDUTIndex() {
    return completedDUTIndex;
}
