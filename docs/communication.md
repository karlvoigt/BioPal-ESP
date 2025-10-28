# Communication Protocols

## Overview

BioPal ESP32 uses three communication interfaces:
- **UART1** (3600 baud): Binary protocol for STM32 communication
- **BLE** (Bluetooth LE): Wireless control from mobile/web apps
- **USB Serial** (115200 baud): Debug console, CSV export, command interface

---

## UART1 - STM32 Communication

### Connection Settings
- **Baud Rate**: 3600 baud (intentionally slow for reliability)
- **Data Bits**: 8
- **Stop Bits**: 1
- **Parity**: None
- **Flow Control**: None
- **Pins**: GPIO 3 (TX), GPIO 2 (RX)
- **Buffer**: 512-byte circular buffer (interrupt-driven)

### Command Protocol (ESP32 → STM32)

#### Command Packet Format
**Size**: 15 bytes

```
┌──────┬──────────┬────────┬────────┬────────┬──────┐
│ 0xAA │ cmd_type │ data1  │ data2  │ data3  │ 0x55 │
├──────┼──────────┼────────┼────────┼────────┼──────┤
│ 1B   │ 1B       │ 4B     │ 4B     │ 4B     │ 1B   │
└──────┴──────────┴────────┴────────┴────────┴──────┘

Total: 15 bytes
- Start delimiter: 0xAA
- Command type: 1 byte
- Data fields: 3 × 4-byte uint32_t (little-endian)
- End delimiter: 0x55
```

#### Command Types

##### 1. CMD_START_MEASUREMENT (0x03)
Start impedance measurement sweep.

**Implementation**: `UART_Functions.cpp:155-175`

**Parameters**:
- `data1`: Number of DUTs to measure (1-4)
- `data2`: Start frequency index (0-37)
- `data3`: End frequency index (0-37)

**Example** (Start 4 DUTs, all frequencies):
```
Hex: AA 03 04 00 00 00 00 00 00 00 25 00 00 00 55
     └┘ └┘ └────────┘ └────────┘ └────────┘ └┘
     │  │      4          0          37      │
   Start CMD    DUTs    StartIDX   EndIDX   End
```

**C++ Code**:
```cpp
void sendStartCommand(uint8_t num_duts, uint8_t startIdx, uint8_t endIdx) {
    uint8_t packet[15] = {0xAA, 0x03};

    // data1: num_duts (little-endian uint32)
    packet[2] = num_duts;
    packet[3] = 0x00;
    packet[4] = 0x00;
    packet[5] = 0x00;

    // data2: startIdx
    packet[6] = startIdx;
    packet[7] = 0x00;
    packet[8] = 0x00;
    packet[9] = 0x00;

    // data3: endIdx
    packet[10] = endIdx;
    packet[11] = 0x00;
    packet[12] = 0x00;
    packet[13] = 0x00;

    packet[14] = 0x55;  // End delimiter

    Serial1.write(packet, 15);
}
```

**Expected Response**: ACK packet (0x06)

---

##### 2. CMD_STOP_MEASUREMENT (0x04)
Stop ongoing measurement.

**Implementation**: `UART_Functions.cpp:177-189`

**Parameters**: All unused (set to 0)

**Example**:
```
Hex: AA 04 00 00 00 00 00 00 00 00 00 00 00 00 55
```

---

##### 3. CMD_SET_PGA_GAIN (0x01)
Set the PGA113 amplifier gain (manual override).

**Implementation**: `UART_Functions.cpp:191-212`

**Parameters**:
- `data1`: Gain value (1, 2, 5, 10, 20, 50, 100, or 200)
- `data2`: Unused
- `data3`: Unused

**Example** (Set gain to 100):
```
Hex: AA 01 64 00 00 00 00 00 00 00 00 00 00 00 55
         └────────┘
            100
```

---

##### 4. CMD_SET_TIA_GAIN (0x05)
Set the Transimpedance Amplifier gain.

**Implementation**: `UART_Functions.cpp:214-235`

**Parameters**:
- `data1`: 0 = High gain (7500Ω), 1 = Low gain (37.5Ω)
- `data2`: Unused
- `data3`: Unused

**Example** (Set to low gain):
```
Hex: AA 05 01 00 00 00 00 00 00 00 00 00 00 00 55
         └────────┘
             1
```

---

### Data Reception Protocol (STM32 → ESP32)

#### Packet Types

##### 1. ACK Packet (0x06)
Acknowledgment of START command.

**Size**: 4 bytes

```
┌──────┬──────┬──────┬──────┐
│ 0xAA │ 0x06 │ 0x01 │ 0x55 │
└──────┴──────┴──────┴──────┘
```

**Received**: Immediately after sending CMD_START_MEASUREMENT

---

##### 2. DUT_START Packet (0x10)
Indicates beginning of data for a DUT.

**Size**: 7 bytes

```
┌──────┬──────┬────────────┬────────────┬──────────┬──────┐
│ 0xAA │ 0x10 │ dut_number │ freq_count │ reserved │ 0x55 │
├──────┼──────┼────────────┼────────────┼──────────┼──────┤
│ 1B   │ 1B   │ 1B         │ 1B         │ 2B       │ 1B   │
└──────┴──────┴────────────┴────────────┴──────────┴──────┘
```

**Fields**:
- `dut_number`: 1-4 (which DUT is starting)
- `freq_count`: Number of frequency points to follow (typically 38)
- `reserved`: Unused (0x00 0x00)

**Example** (DUT 1, 38 frequencies):
```
Hex: AA 10 01 26 00 00 55
         └┘ └┘ └───┘ └┘
       DUT=1 38  Rsvd End
```

**Processing** (`UART_Functions.cpp:88-102`):
```cpp
case READING_DUT_START:
    if (packetIndex == 0) {
        currentDUT = byte;  // DUT number
    } else if (packetIndex == 1) {
        expectedFreqCount = byte;  // Frequency count
    }
    // ... validate and signal
    break;
```

---

##### 3. FREQUENCY_DATA Packet (0x11)
Measurement data for one frequency point.

**Size**: 26 bytes

```
┌──────┬──────┬──────────┬────────┬─────────┬────────┬─────────┬──────────┬──────────┬───────┬──────┐
│ 0xAA │ 0x11 │ freq_hz  │ V_mag  │ V_phase │ I_mag  │ I_phase │ pga_gain │ tia_gain │ valid │ 0x55 │
├──────┼──────┼──────────┼────────┼─────────┼────────┼─────────┼──────────┼──────────┼───────┼──────┤
│ 1B   │ 1B   │ 4B       │ 4B     │ 4B      │ 4B     │ 4B      │ 1B       │ 1B       │ 1B    │ 1B   │
└──────┴──────┴──────────┴────────┴─────────┴────────┴─────────┴──────────┴──────────┴───────┴──────┘

Total: 26 bytes
```

**Field Descriptions**:
- **freq_hz** (uint32_t): Frequency in Hz (little-endian)
- **V_mag** (uint32_t): Voltage magnitude × 1000 (mV)
- **V_phase** (int32_t): Voltage phase × 100 (degrees × 100)
- **I_mag** (uint32_t): Current magnitude × 1000 (mA after TIA conversion)
- **I_phase** (int32_t): Current phase × 100 (degrees × 100)
- **pga_gain** (uint8_t): PGA gain enum (0-7):
  - 0 = gain 1
  - 1 = gain 2
  - 2 = gain 5
  - 3 = gain 10
  - 4 = gain 20
  - 5 = gain 50
  - 6 = gain 100
  - 7 = gain 200
- **tia_gain** (uint8_t): TIA gain setting
  - 0 = High gain (7500Ω)
  - 1 = Low gain (37.5Ω)
- **valid** (uint8_t): Data validity flag
  - 1 = Valid measurement
  - 0 = Invalid (e.g., ADC overload)

**Example** (100 Hz, V=1000mV, I=100mA, PGA=10, TIA=high):
```
Hex: AA 11 64 00 00 00 E8 03 00 00 10 27 00 00 64 00 00 00 20 4E 00 00 03 00 01 55
         └────────┘ └────────┘ └────────┘ └────────┘ └────────┘ └┘ └┘ └┘ └┘
           100 Hz    1000 mV    10000      100 mA     20000      3  0  1  End
                               (100.00°)             (200.00°)  PGA TIA Valid
```

**Processing** (`UART_Functions.cpp:104-145`):
```cpp
case READING_FREQUENCY:
    frequencyData[packetIndex] = byte;

    if (packetIndex == 23) {  // All 24 bytes received
        // Parse into MeasurementPoint
        MeasurementPoint mp;
        memcpy(&mp.freq_hz, &frequencyData[0], 4);

        uint32_t v_mag_raw, i_mag_raw;
        int32_t v_phase_raw, i_phase_raw;

        memcpy(&v_mag_raw, &frequencyData[4], 4);
        memcpy(&v_phase_raw, &frequencyData[8], 4);
        memcpy(&i_mag_raw, &frequencyData[12], 4);
        memcpy(&i_phase_raw, &frequencyData[16], 4);

        mp.V_magnitude = v_mag_raw / 1000.0f;   // mV → V
        mp.V_phase = v_phase_raw / 100.0f;       // degrees
        mp.I_magnitude = i_mag_raw / 1000.0f;    // mA → A
        mp.I_phase = i_phase_raw / 100.0f;

        mp.pga_gain = frequencyData[20];
        mp.tia_gain = frequencyData[21];
        mp.valid = (frequencyData[22] == 1);

        // Queue for processing
        xQueueSend(measurementQueue, &mp, portMAX_DELAY);

        frequencyCounter++;
    }
    break;
```

---

##### 4. DUT_END Packet (0x12)
Indicates end of data for a DUT.

**Size**: 4 bytes

```
┌──────┬──────┬────────────┬──────┐
│ 0xAA │ 0x12 │ dut_number │ 0x55 │
├──────┼──────┼────────────┼──────┤
│ 1B   │ 1B   │ 1B         │ 1B   │
└──────┴──────┴────────────┴──────┘
```

**Example** (DUT 1 complete):
```
Hex: AA 12 01 55
         └┘ └┘
       DUT=1 End
```

**Processing** (`UART_Functions.cpp:147-153`):
```cpp
case READING_DUT_END:
    if (byte == currentDUT) {
        // Signal DUT completion
        xSemaphoreGive(dutCompleteSemaphore);

        if (currentDUT == numDUTs) {
            // All DUTs done
            xSemaphoreGive(measurementCompleteSemaphore);
        }
    }
    break;
```

---

### UART State Machine

**States** (`UART_Functions.cpp:44-51`):
```cpp
enum UARTState {
    WAITING_START,          // Waiting for 0xAA
    READING_PACKET_TYPE,    // Read packet type (0x10, 0x11, 0x12, 0x06)
    READING_DUT_START,      // Parse DUT_START (7 bytes)
    READING_FREQUENCY,      // Parse FREQ_DATA (26 bytes)
    READING_DUT_END,        // Parse DUT_END (4 bytes)
    VALIDATING_END          // Check for 0x55
};
```

**State Transitions**:
```
WAITING_START
    ↓ (0xAA received)
READING_PACKET_TYPE
    ├─> (0x06) → ACK received → WAITING_START
    ├─> (0x10) → READING_DUT_START → VALIDATING_END
    ├─> (0x11) → READING_FREQUENCY → VALIDATING_END
    └─> (0x12) → READING_DUT_END → VALIDATING_END
        ↓
VALIDATING_END
    ├─> (0x55 received, valid) → WAITING_START
    └─> (invalid) → ERROR, reset to WAITING_START
```

**Error Handling**:
- Invalid start delimiter: Ignore byte, continue waiting
- Invalid packet type: Log error, reset to WAITING_START
- Invalid end delimiter: Log error, discard packet, reset
- Timeout: 10-second timeout in taskUARTReader, reset state machine

---

### Complete Measurement Sequence

```
[ESP32 → STM32] CMD_START_MEASUREMENT (4 DUTs, 0-37)
[STM32 → ESP32] ACK (0x06)

[STM32 → ESP32] DUT_START (DUT 1, 38 frequencies)
[STM32 → ESP32] FREQ_DATA (1 Hz)
[STM32 → ESP32] FREQ_DATA (2 Hz)
...
[STM32 → ESP32] FREQ_DATA (100 kHz)
[STM32 → ESP32] DUT_END (DUT 1)

[STM32 → ESP32] DUT_START (DUT 2, 38 frequencies)
[STM32 → ESP32] FREQ_DATA (1 Hz)
...
[STM32 → ESP32] DUT_END (DUT 2)

[STM32 → ESP32] DUT_START (DUT 3, 38 frequencies)
...
[STM32 → ESP32] DUT_END (DUT 3)

[STM32 → ESP32] DUT_START (DUT 4, 38 frequencies)
...
[STM32 → ESP32] DUT_END (DUT 4)

[Measurement complete]
```

**Timing**:
- Total data: ~4 KB (4 DUTs × 38 freq × 26 bytes)
- Transfer time: ~13 seconds @ 3600 baud
- Measurement time: ~42 seconds per DUT (controlled by STM32)
- Total time: ~3 minutes for 4 DUTs

---

## BLE - Wireless Communication

### Service Configuration

**Implementation**: `BLE_Functions.cpp:48-123`

```
Device Name:        BioPal-ESP32
Service UUID:       12345678-1234-5678-1234-56789abcdef0

Characteristics:
  RX (Client → ESP32):
    UUID:           12345678-1234-5678-1234-56789abcdef1
    Properties:     WRITE
    Max Length:     256 bytes

  TX (ESP32 → Client):
    UUID:           12345678-1234-5678-1234-56789abcdef2
    Properties:     NOTIFY
    Max Length:     512 bytes (MTU 517)
```

### Command Protocol (Mobile App → ESP32)

Commands are sent as ASCII strings to the RX characteristic.

#### 1. BASELINE_START
Start baseline measurement.

**Format**:
```
BASELINE_START[,num_duts[,start_idx,end_idx]]
```

**Examples**:
```
BASELINE_START                 → 4 DUTs, all frequencies (0-37)
BASELINE_START,2               → 2 DUTs, all frequencies
BASELINE_START,4,0,20          → 4 DUTs, frequencies 0-20
BASELINE_START,1,10,30         → 1 DUT, frequencies 10-30
```

**Processing** (`main.cpp:264-291`):
```cpp
if (command.startsWith("BASELINE_START")) {
    // Parse optional parameters
    int num_duts = 4;
    int start_idx = 0;
    int end_idx = 37;

    // ... parse from comma-separated string

    // Send START command to STM32
    sendStartCommand(num_duts, start_idx, end_idx);

    // Update GUI state
    setGUIState(GUI_BASELINE_PROGRESS);
}
```

---

#### 2. MEAS_START
Start final measurement (requires baseline first).

**Format**:
```
MEAS_START
```

**Processing**:
```cpp
if (command == "MEAS_START") {
    if (!baselineMeasurementDone) {
        sendBLEStatus("ERROR:No baseline measurement");
        return;
    }

    sendStartCommand(numDUTs, startFreqIndex, endFreqIndex);
    setGUIState(GUI_FINAL_PROGRESS);
}
```

---

#### 3. STOP
Stop ongoing measurement.

**Format**:
```
STOP
```

**Processing**:
```cpp
if (command == "STOP") {
    sendStopCommand();  // To STM32
    setGUIState(GUI_HOME);
    sendBLEStatus("STATUS:Stopped");
}
```

---

### Response Protocol (ESP32 → Mobile App)

Responses are sent as ASCII strings via the TX characteristic (notifications).

#### 1. Status Messages
```
STATUS:ready                    → System ready for commands
STATUS:Measuring:N              → Measuring N DUTs
STATUS:Baseline Complete        → Baseline measurement done
STATUS:Measurement Complete     → Final measurement done
STATUS:Stopped                  → Measurement stopped by user
```

**Implementation**: `BLE_Functions.cpp:259-273`

---

#### 2. DUT Start/End Messages
```
DUT_START:N                     → Starting DUT N
DUT_END:N                       → DUT N complete
```

**Sent**: When dutCompleteSemaphore signals

---

#### 3. Impedance Data (JSON)
```json
DATA:{
  "dut": 1,
  "count": 38,
  "data": [
    {"f": 1, "z": 67234.2, "p": -40.23},
    {"f": 2, "z": 40512.8, "p": -45.12},
    ...
    {"f": 100000, "z": 15.2, "p": -10.34}
  ]
}
```

**Fields**:
- `dut`: DUT number (1-4)
- `count`: Number of data points
- `data`: Array of impedance points
  - `f`: Frequency (Hz)
  - `z`: Impedance magnitude (Ohms)
  - `p`: Phase (degrees)

**Implementation**: `BLE_Functions.cpp:226-257`
```cpp
void sendBLEImpedanceData(uint8_t dutNum, ImpedancePoint data[], int count) {
    DynamicJsonDocument doc(8192);  // 8 KB buffer

    doc["dut"] = dutNum;
    doc["count"] = count;

    JsonArray dataArray = doc.createNestedArray("data");
    for (int i = 0; i < count; i++) {
        JsonObject point = dataArray.createNestedObject();
        point["f"] = data[i].freq_hz;
        point["z"] = data[i].magnitude;
        point["p"] = data[i].phase;
    }

    String json;
    serializeJson(doc, json);

    String message = "DATA:" + json;
    pTxCharacteristic->setValue(message.c_str());
    pTxCharacteristic->notify();
}
```

**Packet Size**: Typically 2-4 KB per DUT (depends on precision)

---

#### 4. Error Messages
```
ERROR:No baseline measurement     → Tried to start final without baseline
ERROR:Invalid command              → Unknown BLE command
ERROR:STM32 communication error    → UART timeout or invalid data
ERROR:Calibration file not found   → Missing calibration.csv
```

---

### BLE Connection Management

**Connection Events** (`BLE_Functions.cpp:27-46`):
```cpp
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("BLE: Client connected");
        sendBLEStatus("STATUS:ready");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("BLE: Client disconnected");

        // Restart advertising
        pServer->startAdvertising();
    }
};
```

**Advertising**:
- Starts automatically on boot
- Restarts automatically after disconnect
- 20-40ms interval (fast discovery)

---

## USB Serial - Debug & Export

### Connection Settings
- **Interface**: USB-CDC (native ESP32-C6 USB)
- **Baud Rate**: 115200
- **Data Bits**: 8
- **Stop Bits**: 1
- **Parity**: None

### Command Interface

Commands are sent as ASCII text lines.

**Implementation**: `serial_commands.cpp:12-64`

#### Commands

##### 1. start [num_duts]
Start measurement.

**Examples**:
```
start           → Start with 4 DUTs (default)
start 1         → Start with 1 DUT
start 2         → Start with 2 DUTs
```

**Response**:
```
Starting measurement with N DUTs...
```

---

##### 2. stop
Stop ongoing measurement.

**Example**:
```
stop
```

**Response**:
```
Measurement stopped.
```

---

##### 3. help
Show available commands.

**Example**:
```
help
```

**Response**:
```
Available commands:
  start [num_duts]  - Start measurement (default: 4 DUTs)
  stop              - Stop measurement
  help              - Show this help
```

---

### CSV Data Export

**Format**: Comma-separated values

**Implementation**: `csv_export.cpp:7-23`

**Header**:
```
DUT,Frequency_Hz,Magnitude_Ohms,Phase_Deg
```

**Data Rows**:
```
1,1,67234.2,-40.23
1,2,40512.8,-45.12
...
4,100000,15.2,-10.34
```

**Export Trigger**: Automatic after measurement completes

**Example Output**:
```
DUT,Frequency_Hz,Magnitude_Ohms,Phase_Deg
1,1.0,67234.2345,-40.23
1,2.0,40512.8012,-45.12
1,4.0,28456.3421,-48.56
...
4,80000.0,18.7654,-8.92
4,100000.0,15.2345,-10.34

Measurement complete. 152 data points exported.
```

---

### Debug Console Output

**Startup Messages**:
```
BioPal ESP32 Starting...
Mounting LittleFS...
Loading calibration data from /calibration.csv...
Loaded 608 calibration points.
Initializing UART to STM32 @ 3600 baud...
Initializing BLE...
BLE: Device name: BioPal-ESP32
Creating FreeRTOS tasks...
System initialized. Ready for measurement.
```

**During Measurement**:
```
UART: DUT_START received - DUT 1, 38 frequencies
UART: FREQ_DATA received - 1 Hz
  Z = 67234.23 Ω, Phase = -40.23°
UART: FREQ_DATA received - 2 Hz
  Z = 40512.80 Ω, Phase = -45.12°
...
UART: DUT_END received - DUT 1 complete
BLE: Sent DUT 1 data (2.4 KB)
```

**Errors**:
```
ERROR: UART timeout waiting for data
ERROR: Invalid packet - expected 0x55, got 0x3F
ERROR: Calibration file not found: /calibration.csv
ERROR: Queue full - dropping measurement point
```

---

## Communication Timing

### UART Throughput
- **Baud Rate**: 3600 baud = 450 bytes/sec theoretical
- **Actual**: ~300 bytes/sec (overhead, delays)
- **Single Frequency**: 26 bytes = ~87ms transfer time
- **38 Frequencies**: 26 × 38 = 988 bytes = ~3.3 seconds
- **4 DUTs**: 988 × 4 = ~13 seconds total data transfer

### BLE Throughput
- **Notification Rate**: ~100 notifications/sec
- **Packet Size**: ~512 bytes max (MTU 517)
- **Throughput**: ~10 KB/sec
- **Single DUT Data**: 2-4 KB = ~400ms transmission time

### USB Serial Throughput
- **Baud Rate**: 115200 baud = 14,400 bytes/sec theoretical
- **CSV Export**: 152 points × 40 bytes/line = ~6 KB
- **Transfer Time**: ~500ms

---

## Protocol Comparison

| Feature | UART | BLE | USB Serial |
|---------|------|-----|------------|
| **Speed** | 3600 baud | ~10 KB/sec | 115200 baud |
| **Direction** | Bidirectional | Bidirectional | Bidirectional |
| **Format** | Binary | ASCII/JSON | ASCII |
| **Usage** | STM32 control/data | Mobile app control | Debug/export |
| **Reliability** | High | Medium | High |
| **Range** | Wired | ~10m | Wired |
| **Latency** | Low | Medium | Low |

---

## Error Recovery

### UART Errors
1. **Invalid Packet**: Log error, reset state machine, continue
2. **Timeout**: Wait 10 seconds, reset state, signal error
3. **Buffer Overflow**: Drop oldest data (rare with 512B buffer)
4. **Checksum Mismatch**: Currently no checksum (could be added)

### BLE Errors
1. **Disconnection**: Automatically restart advertising
2. **MTU Negotiation Failed**: Fall back to smaller packets
3. **Notification Failed**: Retry once, then log error
4. **Invalid Command**: Send ERROR message, ignore command

### USB Serial Errors
1. **Not Connected**: Silently skip debug output (non-critical)
2. **Buffer Full**: Block until space available
3. **Invalid Command**: Print error message, show help

---

## Best Practices

### For Developers

1. **UART Communication**:
   - Always validate start (0xAA) and end (0x55) delimiters
   - Use interrupt-driven reception (don't poll)
   - Keep ISR fast (just buffer bytes)
   - Process packets in task context

2. **BLE Communication**:
   - Keep notifications under 512 bytes (MTU limit)
   - Use JSON for structured data (easier debugging)
   - Always check deviceConnected before sending
   - Handle disconnections gracefully

3. **USB Serial**:
   - Use for debugging only (not time-critical)
   - Print errors and warnings clearly
   - Export data in standard CSV format

### For Users

1. **UART Connection**:
   - Ensure TX/RX are crossed (ESP TX → STM RX, ESP RX → STM TX)
   - Verify baud rate is 3600 on both sides
   - Check ground connection

2. **BLE Connection**:
   - Ensure Bluetooth is enabled on mobile device
   - Look for "BioPal-ESP32" in BLE scan
   - Wait for "STATUS:ready" before sending commands

3. **USB Serial**:
   - Connect USB cable to computer
   - Open serial monitor at 115200 baud
   - Type commands and press Enter
