#ifndef UART_STM32_H
#define UART_STM32_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <vector>

// UART Command Definitions (matching STM32 uart_commands.h)
#define CMD_SET_PGA_GAIN        0x01
#define CMD_SET_MUX_CHANNEL     0x02
#define CMD_START_MEASUREMENT   0x03
#define CMD_END_MEASUREMENT     0x04

#define UART_CMD_START_BYTE     0xAA
#define UART_CMD_END_BYTE       0x55
#define UART_CMD_PACKET_SIZE    15

// UART Data Packet Definitions (matching STM32 uart_data_tx.h)
#define UART_DATA_START_BYTE    0xAA
#define UART_DATA_END_BYTE      0x55
#define UART_DATA_DUT_START     0x10
#define UART_DATA_FREQUENCY     0x11
#define UART_DATA_DUT_END       0x12
#define UART_DATA_DUT_START_SIZE    7
#define UART_DATA_FREQUENCY_SIZE    25
#define UART_DATA_DUT_END_SIZE      4

// UART pins (ESP32-C6)
#define STM32_UART_TX           GPIO_NUM_3
#define STM32_UART_RX           GPIO_NUM_2
#define STM32_UART_BAUD         115200

struct MeasurementPoint {
    uint32_t freq_hz;
    float magnitude;    // Converted from magnitude*1000
    float phase_deg;    // Converted from phase*100
    uint8_t pga_gain;   // PGA gain value (1, 2, 5, 10, 20, 50, 100, 200)
    bool valid;
};

struct DUTResults {
    std::vector<MeasurementPoint> voltage;
    std::vector<MeasurementPoint> current;
    uint8_t dut_number;
    bool valid;
};

class UART_STM32 {
public:
    UART_STM32();
    bool begin();
    void sendCommand(uint8_t cmd_type, uint32_t data1 = 0, uint32_t data2 = 0, uint32_t data3 = 0);
    void sendStartMeasurement();
    void sendStopMeasurement();
    void setPGAGain(uint8_t gain);

    // Data reception (binary packets)
    bool receiveDUTResults(uint8_t dut_num, DUTResults& results);

    // Legacy text-based reception (kept for compatibility)
    bool waitForMarker(const char* marker, uint32_t timeout_ms = 120000);
    bool parseDUTResults(uint8_t dut_num, DUTResults& results);
    String readLine(uint32_t timeout_ms = 1000);

    // Helper
    void flushInput();

private:
    HardwareSerial* uart;
    void sendPacket(uint8_t cmd_type, uint32_t data1, uint32_t data2, uint32_t data3);

    // Binary packet reception helpers
    bool readByte(uint8_t& byte, uint32_t timeout_ms);
    bool readBytes(uint8_t* buffer, size_t length, uint32_t timeout_ms);
    uint32_t extractUint32LE(const uint8_t* buffer);
    int32_t extractInt32LE(const uint8_t* buffer);
};

#endif // UART_STM32_H
