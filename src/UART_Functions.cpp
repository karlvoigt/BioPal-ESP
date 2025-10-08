#include "UART_Functions.h"

// UART serial instance
HardwareSerial UARTSerial(1);  // Use Serial1 for ESP32-C6

// Queue handle for sending measurement points to processing task
static QueueHandle_t measurementQueueHandle = nullptr;

// Semaphore for signaling when data is received
static SemaphoreHandle_t uartDataSemaphore = nullptr;

// Circular buffer for ISR byte collection
#define CIRC_BUFFER_SIZE 512
static uint8_t circBuffer[CIRC_BUFFER_SIZE];
static volatile uint16_t circBufferHead = 0;
static volatile uint16_t circBufferTail = 0;

// Receiver context (used by processing task, not ISR)
static UARTRxContext rxContext;

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

    // Create semaphore for signaling
    uartDataSemaphore = xSemaphoreCreateBinary();

    // Initialize UART with 115200 baud on pins 2 (RX) and 3 (TX)
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

void sendCommand(uint8_t cmd_type, uint32_t data1, uint32_t data2, uint32_t data3) {
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
}

void sendStartCommand() {
    Serial.println("Sending START command to STM32");
    sendCommand(CMD_START_MEASUREMENT, 0, 0, 0);
}

void sendStopCommand() {
    Serial.println("Sending STOP command to STM32");
    sendCommand(CMD_END_MEASUREMENT, 0, 0, 0);
}

void sendSetPGAGainCommand(uint8_t gain) {
    Serial.printf("Sending SET_PGA_GAIN command: %d\n", gain);
    sendCommand(CMD_SET_PGA_GAIN, gain, 0, 0);
}

void sendSetMuxChannelCommand(uint8_t channel) {
    Serial.printf("Sending SET_MUX_CHANNEL command: %d\n", channel);
    sendCommand(CMD_SET_MUX_CHANNEL, channel, 0, 0);
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

// Parse and queue a frequency packet
static void parseFrequencyPacket() {
    MeasurementPoint point;

    // Parse frequency (4 bytes at index 2)
    point.freq_hz = bytesToUint32(&rxContext.buffer[2]);

    // Parse voltage magnitude (4 bytes at index 6) - scaled by 1000
    uint32_t v_mag_scaled = bytesToUint32(&rxContext.buffer[6]);
    point.V_magnitude = v_mag_scaled / 1000.0f;

    // Parse voltage phase (4 bytes at index 10) - scaled by 100
    int32_t v_phase_scaled = bytesToInt32(&rxContext.buffer[10]);
    float v_phase = v_phase_scaled / 100.0f;

    // Parse current magnitude (4 bytes at index 14) - scaled by 1000
    uint32_t i_mag_scaled = bytesToUint32(&rxContext.buffer[14]);
    point.I_magnitude = i_mag_scaled / 1000.0f;

    // Parse current phase (4 bytes at index 18) - scaled by 100
    int32_t i_phase_scaled = bytesToInt32(&rxContext.buffer[18]);
    float i_phase = i_phase_scaled / 100.0f;

    // Calculate phase difference (V - I)
    point.phase_deg = v_phase - i_phase;

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

            // Determine packet type and expected size
            if (byte == UART_DATA_DUT_START) {
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
                    // Process complete packet
                    if (rxContext.packetType == UART_DATA_DUT_START) {
                        rxContext.currentDUT = rxContext.buffer[2];
                        rxContext.expectedFreqCount = rxContext.buffer[3];
                        Serial.printf("\n=== DUT %d START (expecting %d frequencies) ===\n",
                                     rxContext.currentDUT, rxContext.expectedFreqCount);
                    }
                    else if (rxContext.packetType == UART_DATA_FREQUENCY) {
                        parseFrequencyPacket();
                    }
                    else if (rxContext.packetType == UART_DATA_DUT_END) {
                        Serial.printf("=== DUT %d END ===\n\n", rxContext.buffer[2]);
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
