#include "uart_stm32.h"

UART_STM32::UART_STM32() {
    uart = nullptr;
}

bool UART_STM32::begin() {
    // Initialize Hardware UART1 (GPIO2=RX, GPIO3=TX for ESP32-C6)
    uart = new HardwareSerial(1);
    uart->begin(STM32_UART_BAUD, SERIAL_8N1, STM32_UART_RX, STM32_UART_TX);

    delay(100);  // Let UART stabilize
    flushInput();

    Serial.println("STM32 UART initialized on GPIO2(RX)/GPIO3(TX)");
    return true;
}

void UART_STM32::sendPacket(uint8_t cmd_type, uint32_t data1, uint32_t data2, uint32_t data3) {
    uint8_t packet[UART_CMD_PACKET_SIZE];

    packet[0] = UART_CMD_START_BYTE;
    packet[1] = cmd_type;

    // Pack data1, data2, data3 as little-endian uint32_t
    packet[2] = (data1 >> 0) & 0xFF;
    packet[3] = (data1 >> 8) & 0xFF;
    packet[4] = (data1 >> 16) & 0xFF;
    packet[5] = (data1 >> 24) & 0xFF;

    packet[6] = (data2 >> 0) & 0xFF;
    packet[7] = (data2 >> 8) & 0xFF;
    packet[8] = (data2 >> 16) & 0xFF;
    packet[9] = (data2 >> 24) & 0xFF;

    packet[10] = (data3 >> 0) & 0xFF;
    packet[11] = (data3 >> 8) & 0xFF;
    packet[12] = (data3 >> 16) & 0xFF;
    packet[13] = (data3 >> 24) & 0xFF;

    packet[14] = UART_CMD_END_BYTE;

    uart->write(packet, UART_CMD_PACKET_SIZE);
    uart->flush();

    Serial.printf("Sent command: 0x%02X, data1=%lu\n", cmd_type, data1);
}

void UART_STM32::sendCommand(uint8_t cmd_type, uint32_t data1, uint32_t data2, uint32_t data3) {
    sendPacket(cmd_type, data1, data2, data3);
}

void UART_STM32::sendStartMeasurement() {
    Serial.println("Sending START_MEASUREMENT command to STM32...");
    sendPacket(CMD_START_MEASUREMENT, 0, 0, 0);
}

void UART_STM32::sendStopMeasurement() {
    Serial.println("Sending STOP_MEASUREMENT command to STM32...");
    sendPacket(CMD_END_MEASUREMENT, 0, 0, 0);
}

void UART_STM32::setPGAGain(uint8_t gain) {
    Serial.printf("Setting PGA gain to %d\n", gain);
    sendPacket(CMD_SET_PGA_GAIN, gain, 0, 0);
}

void UART_STM32::flushInput() {
    while (uart->available()) {
        uart->read();
    }
}

String UART_STM32::readLine(uint32_t timeout_ms) {
    String line = "";
    uint32_t start_time = millis();

    while (millis() - start_time < timeout_ms) {
        if (uart->available()) {
            char c = uart->read();
            if (c == '\n') {
                return line;
            } else if (c != '\r') {
                line += c;
            }
        }
        delay(1);  // Small delay to prevent tight loop
    }

    return line;  // Return partial line on timeout
}

bool UART_STM32::waitForMarker(const char* marker, uint32_t timeout_ms) {
    Serial.printf("Waiting for marker: '%s' (timeout: %lu ms)\n", marker, timeout_ms);
    uint32_t start_time = millis();

    while (millis() - start_time < timeout_ms) {
        String line = readLine(1000);
        if (line.length() > 0) {
            Serial.printf("  RX: %s\n", line.c_str());
            if (line.indexOf(marker) >= 0) {
                Serial.printf("Found marker: '%s'\n", marker);
                return true;
            }
        }
    }

    Serial.printf("Timeout waiting for marker: '%s'\n", marker);
    return false;
}

bool UART_STM32::parseDUTResults(uint8_t dut_num, DUTResults& results) {
    results.voltage.clear();
    results.current.clear();
    results.dut_number = dut_num;
    results.valid = false;

    // Wait for voltage header: "DUT_X_VOLTAGE,N"
    char voltage_marker[32];
    snprintf(voltage_marker, sizeof(voltage_marker), "DUT_%d_VOLTAGE", dut_num);

    if (!waitForMarker(voltage_marker, 120000)) {
        Serial.printf("Failed to find voltage marker for DUT %d\n", dut_num);
        return false;
    }

    // Parse voltage header to get count
    String header_line = readLine(1000);
    int comma_pos = header_line.indexOf(',');
    if (comma_pos < 0) {
        Serial.println("Invalid voltage header format");
        return false;
    }

    int voltage_count = header_line.substring(comma_pos + 1).toInt();
    Serial.printf("Expecting %d voltage measurements\n", voltage_count);

    // Parse voltage data lines
    for (int i = 0; i < voltage_count; i++) {
        String line = readLine(5000);
        if (line.length() == 0) {
            Serial.printf("Timeout reading voltage line %d\n", i);
            return false;
        }

        // Parse CSV: freq_hz,magnitude_x1000,phase_x100,valid
        int pos1 = line.indexOf(',');
        int pos2 = line.indexOf(',', pos1 + 1);
        int pos3 = line.indexOf(',', pos2 + 1);

        if (pos1 < 0 || pos2 < 0 || pos3 < 0) {
            Serial.printf("Invalid voltage line format: %s\n", line.c_str());
            continue;
        }

        MeasurementPoint point;
        point.freq_hz = line.substring(0, pos1).toInt();
        point.magnitude = line.substring(pos1 + 1, pos2).toFloat() / 1000.0f;
        point.phase_deg = line.substring(pos2 + 1, pos3).toFloat() / 100.0f;
        point.valid = (line.substring(pos3 + 1).toInt() == 1);

        results.voltage.push_back(point);
    }

    // Wait for current header: "DUT_X_CURRENT,N"
    char current_marker[32];
    snprintf(current_marker, sizeof(current_marker), "DUT_%d_CURRENT", dut_num);

    if (!waitForMarker(current_marker, 10000)) {
        Serial.printf("Failed to find current marker for DUT %d\n", dut_num);
        return false;
    }

    // Parse current header
    header_line = readLine(1000);
    comma_pos = header_line.indexOf(',');
    if (comma_pos < 0) {
        Serial.println("Invalid current header format");
        return false;
    }

    int current_count = header_line.substring(comma_pos + 1).toInt();
    Serial.printf("Expecting %d current measurements\n", current_count);

    // Parse current data lines
    for (int i = 0; i < current_count; i++) {
        String line = readLine(5000);
        if (line.length() == 0) {
            Serial.printf("Timeout reading current line %d\n", i);
            return false;
        }

        // Parse CSV: freq_hz,magnitude_x1000,phase_x100,valid
        int pos1 = line.indexOf(',');
        int pos2 = line.indexOf(',', pos1 + 1);
        int pos3 = line.indexOf(',', pos2 + 1);

        if (pos1 < 0 || pos2 < 0 || pos3 < 0) {
            Serial.printf("Invalid current line format: %s\n", line.c_str());
            continue;
        }

        MeasurementPoint point;
        point.freq_hz = line.substring(0, pos1).toInt();
        point.magnitude = line.substring(pos1 + 1, pos2).toFloat() / 1000.0f;
        point.phase_deg = line.substring(pos2 + 1, pos3).toFloat() / 100.0f;
        point.valid = (line.substring(pos3 + 1).toInt() == 1);

        results.current.push_back(point);
    }

    Serial.printf("Successfully parsed DUT %d: %d voltage, %d current points\n",
                  dut_num, results.voltage.size(), results.current.size());

    results.valid = (results.voltage.size() > 0 && results.current.size() > 0);
    return results.valid;
}

// ========== BINARY PACKET RECEPTION ==========

bool UART_STM32::readByte(uint8_t& byte, uint32_t timeout_ms) {
    uint32_t start_time = millis();
    while (millis() - start_time < timeout_ms) {
        if (uart->available()) {
            byte = uart->read();
            return true;
        }
        delay(1);
    }
    return false;
}

bool UART_STM32::readBytes(uint8_t* buffer, size_t length, uint32_t timeout_ms) {
    for (size_t i = 0; i < length; i++) {
        if (!readByte(buffer[i], timeout_ms)) {
            return false;
        }
    }
    return true;
}

uint32_t UART_STM32::extractUint32LE(const uint8_t* buffer) {
    return ((uint32_t)buffer[0] << 0) |
           ((uint32_t)buffer[1] << 8) |
           ((uint32_t)buffer[2] << 16) |
           ((uint32_t)buffer[3] << 24);
}

int32_t UART_STM32::extractInt32LE(const uint8_t* buffer) {
    return (int32_t)extractUint32LE(buffer);
}

bool UART_STM32::receiveDUTResults(uint8_t dut_num, DUTResults& results) {
    results.voltage.clear();
    results.current.clear();
    results.dut_number = dut_num;
    results.valid = false;

    Serial.printf("Waiting for binary DUT %d results...\n", dut_num);

    // Wait for DUT_START packet
    uint8_t start_byte, packet_type;
    uint32_t timeout_ms = 120000;  // 2 minutes timeout
    uint32_t start_time = millis();

    while (millis() - start_time < timeout_ms) {
        if (!readByte(start_byte, 1000)) continue;

        if (start_byte == UART_DATA_START_BYTE) {
            if (!readByte(packet_type, 100)) continue;

            if (packet_type == UART_DATA_DUT_START) {
                // Read DUT_START packet payload
                uint8_t payload[4];
                if (!readBytes(payload, 4, 100)) {
                    Serial.println("Failed to read DUT_START payload");
                    continue;
                }

                uint8_t received_dut = payload[0];
                uint8_t freq_count = payload[1];
                uint8_t end_byte = payload[3];

                if (end_byte != UART_DATA_END_BYTE) {
                    Serial.println("Invalid DUT_START end byte");
                    continue;
                }

                if (received_dut != dut_num) {
                    Serial.printf("DUT mismatch: expected %d, got %d\n", dut_num, received_dut);
                    continue;
                }

                Serial.printf("Received DUT_START for DUT %d, expecting %d frequencies\n", received_dut, freq_count);

                // Receive frequency data packets
                for (uint8_t i = 0; i < freq_count; i++) {
                    // Wait for FREQUENCY packet
                    if (!readByte(start_byte, 5000) || start_byte != UART_DATA_START_BYTE) {
                        Serial.printf("Timeout waiting for FREQUENCY packet %d\n", i);
                        return false;
                    }

                    if (!readByte(packet_type, 100) || packet_type != UART_DATA_FREQUENCY) {
                        Serial.printf("Expected FREQUENCY packet, got 0x%02X\n", packet_type);
                        return false;
                    }

                    // Read FREQUENCY packet payload (22 bytes)
                    uint8_t freq_payload[23];
                    if (!readBytes(freq_payload, 23, 500)) {
                        Serial.printf("Failed to read FREQUENCY packet %d\n", i);
                        return false;
                    }

                    if (freq_payload[22] != UART_DATA_END_BYTE) {
                        Serial.println("Invalid FREQUENCY end byte");
                        return false;
                    }

                    // Parse frequency data
                    uint32_t freq_hz = extractUint32LE(&freq_payload[0]);
                    uint32_t v_mag = extractUint32LE(&freq_payload[4]);
                    int32_t v_phase = extractInt32LE(&freq_payload[8]);
                    uint32_t i_mag = extractUint32LE(&freq_payload[12]);
                    int32_t i_phase = extractInt32LE(&freq_payload[16]);
                    uint8_t pga_gain = freq_payload[20];
                    uint8_t valid = freq_payload[21];

                    // Store voltage measurement
                    MeasurementPoint v_point;
                    v_point.freq_hz = freq_hz;
                    v_point.magnitude = v_mag / 1000.0f;
                    v_point.phase_deg = v_phase / 100.0f;
                    v_point.pga_gain = pga_gain;
                    v_point.valid = (valid == 1);
                    results.voltage.push_back(v_point);

                    // Store current measurement
                    MeasurementPoint i_point;
                    i_point.freq_hz = freq_hz;
                    i_point.magnitude = i_mag / 1000.0f;
                    i_point.phase_deg = i_phase / 100.0f;
                    i_point.pga_gain = pga_gain;
                    i_point.valid = (valid == 1);
                    results.current.push_back(i_point);
                }

                // Wait for DUT_END packet
                if (!readByte(start_byte, 1000) || start_byte != UART_DATA_START_BYTE) {
                    Serial.println("Timeout waiting for DUT_END");
                    return false;
                }

                if (!readByte(packet_type, 100) || packet_type != UART_DATA_DUT_END) {
                    Serial.printf("Expected DUT_END, got 0x%02X\n", packet_type);
                    return false;
                }

                uint8_t end_payload[2];
                if (!readBytes(end_payload, 2, 100)) {
                    Serial.println("Failed to read DUT_END payload");
                    return false;
                }

                if (end_payload[0] != dut_num || end_payload[1] != UART_DATA_END_BYTE) {
                    Serial.println("Invalid DUT_END packet");
                    return false;
                }

                Serial.printf("Successfully received DUT %d: %d frequency points\n",
                              dut_num, results.voltage.size());

                results.valid = (results.voltage.size() > 0 && results.current.size() > 0);
                return results.valid;
            }
        }
    }

    Serial.println("Timeout waiting for DUT_START");
    return false;
}
