#ifndef UART_FUNCTIONS_H
#define UART_FUNCTIONS_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "defines.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// UART configuration
#define UART_RX_PIN         2
#define UART_TX_PIN         3
#define UART_BAUD_RATE      3600

// Command protocol (matching STM32)
#define UART_CMD_START_BYTE     0xAA
#define UART_CMD_END_BYTE       0x55
#define UART_CMD_PACKET_SIZE    15
#define UART_ACK_PACKET_SIZE    4

// Command types
#define CMD_SET_PGA_GAIN        0x01
#define CMD_SET_MUX_CHANNEL     0x02
#define CMD_START_MEASUREMENT   0x03
#define CMD_END_MEASUREMENT     0x04
#define CMD_SET_TIA_GAIN        0x05

// Data packet protocol (matching STM32)
#define UART_DATA_START_BYTE    0xAA
#define UART_DATA_END_BYTE      0x55

// Packet types
#define UART_DATA_DUT_START     0x10
#define UART_DATA_FREQUENCY     0x11
#define UART_DATA_DUT_END       0x12

// Packet sizes
#define UART_DATA_DUT_START_SIZE    7
#define UART_DATA_FREQUENCY_SIZE    26
#define UART_DATA_DUT_END_SIZE      4

// Receive state machine states
enum UARTRxState {
    WAITING_START,
    READING_PACKET_TYPE,
    READING_DUT_START,
    READING_FREQUENCY,
    READING_DUT_END,
    VALIDATING_END
};

// UART receiver context
struct UARTRxContext {
    UARTRxState state;
    uint8_t buffer[32];
    uint8_t byteCount;
    uint8_t expectedBytes;
    uint8_t packetType;
    uint8_t currentDUT;
    uint8_t expectedFreqCount;
};

/*=========================INITIALIZATION=========================*/
// Initialize UART communication with STM32 using Arduino HardwareSerial
// Sets up interrupt-driven reception using onReceive() callback
// measurementQueue: FreeRTOS queue for parsed MeasurementPoints
void initUART(QueueHandle_t measurementQueue);

// Get semaphore handle for signaling when new data received
// Task can wait on this semaphore instead of polling
SemaphoreHandle_t getUARTSemaphore();

// Process buffered bytes from ISR circular buffer
// Call this from a task (not ISR) after semaphore is signaled
// Processes all available bytes through state machine
void processBufferedBytes();

/*=========================COMMAND SENDING=========================*/
// Send start measurement command to STM32 (default 4 DUTs)
bool sendStartCommand();

// Send start measurement command with specific number of DUTs (1-4)
bool sendStartCommand(uint8_t num_duts);

// Send stop measurement command to STM32
bool sendStopCommand();

// Send set PGA gain command
bool sendSetPGAGainCommand(uint8_t gain);

// Send set MUX channel command
bool sendSetMuxChannelCommand(uint8_t channel);

// Send set TIA gain command (0 = high gain, 1 = low gain)
bool sendSetTIAGainCommand(uint8_t low_gain);

// Generic command sender
bool sendCommand(uint8_t cmd_type, uint32_t data1, uint32_t data2, uint32_t data3);

// Wait for ACK packet from STM32 for a specific command
// Returns true if ACK received within timeout, false otherwise
bool waitForAck(uint8_t cmd_type, uint32_t timeout_ms);

/*=========================PACKET RECEIVING=========================*/
// Process incoming UART data (call from UART ISR or task)
void processIncomingByte(uint8_t byte);

// Get current DUT being processed
uint8_t getCurrentDUT();

/*=========================EVENT SIGNALING=========================*/
// Get semaphore signaled when a DUT completes (DUT_END packet received)
// GUI task can wait on this to trigger Bode plot drawing
SemaphoreHandle_t getDUTCompleteSemaphore();

// Get semaphore signaled when all measurements complete
// GUI task can wait on this to trigger CSV export
SemaphoreHandle_t getMeasurementCompleteSemaphore();

// Get the DUT index that just completed (0-3 for DUT 1-4)
// Call after getDUTCompleteSemaphore() signals
uint8_t getCompletedDUTIndex();

#endif // UART_FUNCTIONS_H
