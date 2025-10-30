# BioPal ESP32 System Architecture

## System Purpose

BioPal ESP32 is a bioimpedance analyzer controller that manages multi-channel impedance measurements. It communicates with an STM32 measurement board to orchestrate frequency sweeps, processes and calibrates the measurement data, provides a user interface via TFT display, and offers wireless control through Bluetooth LE.

The ESP32 acts as the **master controller** and **user interface**, while the STM32 acts as the **measurement engine**.

## Project Structure

```
BioPal ESP/
├── src/                              # Source files (11 C++ files, ~3,562 LOC)
│   ├── main.cpp                      # Entry point, task initialization, globals (359 LOC)
│   ├── UART_Functions.cpp            # STM32 communication driver (435 LOC)
│   ├── BLE_Functions.cpp             # Bluetooth LE interface (376 LOC)
│   ├── calibration.cpp               # Calibration engine (963 LOC - largest)
│   ├── gui_state.cpp                 # GUI state machine (373 LOC)
│   ├── gui_screens.cpp               # Display rendering (465 LOC)
│   ├── bode_plot.cpp                 # Bode plot visualization (290 LOC)
│   ├── button_handler.cpp            # Input handling (170 LOC)
│   ├── impedance_calc.cpp            # Z = V/I calculation (31 LOC)
│   ├── serial_commands.cpp           # USB serial CLI (75 LOC)
│   └── csv_export.cpp                # Data export (25 LOC)
├── include/                          # Header files (17 files, ~1,023 LOC)
│   ├── UART_Functions.h
│   ├── BLE_Functions.h
│   ├── calibration.h
│   ├── gui_state.h
│   ├── gui_screens.h
│   ├── bode_plot.h
│   ├── button_handler.h
│   ├── impedance_calc.h
│   ├── pinDefs.h                     # GPIO pin assignments
│   └── ...
├── data/                             # Calibration data files
│   ├── calibration.csv               # Main calibration lookup table
│   ├── voltage.csv                   # Voltage measurement calibration
│   ├── tia_high.csv                  # TIA high-gain (7500Ω) calibration
│   ├── tia_low.csv                   # TIA low-gain (37.5Ω) calibration
│   └── pga_*.csv                     # PGA gain calibration files (1,2,5,10,20,50,100,200)
├── platformio.ini                    # Build configuration
├── TFT_eSPI/                         # TFT display library customization
└── README_*.md                       # Technical documentation
```

## Software Architecture

### FreeRTOS Task Architecture

The application uses a **three-task concurrent architecture** instead of a traditional Arduino loop():

```
┌─────────────────────────────────────────────────────────────┐
│                     main.cpp                                │
│  ┌────────────────────────────────────────────────────┐     │
│  │  setup()                                           │     │
│  │  - Initialize hardware                             │     │
│  │  - Load calibration data                           │     │
│  │  - Create measurement queue                        │     │
│  │  - xTaskCreate(...) × 3                            │     │
│  └────────────────────────────────────────────────────┘     │
│                          │                                   │
│           ┌──────────────┼──────────────┐                   │
│           │              │              │                    │
│           ▼              ▼              ▼                    │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐       │
│  │ taskUART     │ │ taskData     │ │ taskGUI      │       │
│  │ Reader       │ │ Processor    │ │              │       │
│  │ Priority: 2  │ │ Priority: 2  │ │ Priority: 1  │       │
│  │ Stack: 4KB   │ │ Stack: 8KB   │ │ Stack: 4KB   │       │
│  └──────┬───────┘ └──────┬───────┘ └──────┬───────┘       │
│         │                 │                 │                │
└─────────┼─────────────────┼─────────────────┼────────────────┘
          │                 │                 │
          │  Measurement    │    Impedance    │
          │  Queue          │    Data Arrays  │
          └────────────────>└────────────────>│
```

#### Task 1: UART Reader (taskUARTReader)
- **Priority**: 2 (high)
- **Stack**: 4096 bytes
- **Function**: Parse incoming data from STM32
- **Implementation**: `UART_Functions.cpp:461-523`

**Responsibilities**:
1. Wait on UART semaphore (signaled by ISR)
2. Read bytes from circular buffer (512 bytes)
3. Parse binary packets using state machine
4. Queue MeasurementPoint structures to measurementQueue

**State Machine States**:
```
WAITING_START (0x00)
    ↓ (0xAA received)
READING_PACKET_TYPE
    ├─> READING_DUT_START (0x10)
    ├─> READING_FREQUENCY (0x11)
    └─> READING_DUT_END (0x12)
        ↓
VALIDATING_END (0x55)
    ↓
WAITING_START (loop)
```

#### Task 2: Data Processor (taskDataProcessor)
- **Priority**: 2 (high)
- **Stack**: 8192 bytes (large for calibration lookups)
- **Function**: Calculate and calibrate impedance
- **Implementation**: `main.cpp:87-135`

**Responsibilities**:
1. Wait on measurementQueue (blocks until data available)
2. Calculate impedance: Z = V_magnitude / I_magnitude
3. Apply calibration corrections
4. Store results in global impedanceData arrays
5. Increment frequencyCount for current DUT

**Processing Pipeline**:
```
MeasurementPoint (from queue)
    ↓
calcImpedance() → Z_raw = V / I
    ↓
calibrate() → Apply lookup table or formula
    ↓
Store in impedanceData[dutIndex][freqIndex]
    ↓
Increment frequencyCount[dutIndex]
```

#### Task 3: GUI (taskGUI)
- **Priority**: 1 (low - allows UART/processing to preempt)
- **Stack**: 4096 bytes
- **Function**: User interface management
- **Implementation**: `main.cpp:137-262`

**Responsibilities**:
1. Render current GUI state screen
2. Process button events from queue
3. Handle BLE command strings
4. Update progress displays
5. Send BLE/serial data when DUTs complete
6. Manage state transitions

**Update Loop**:
```
while(1) {
    renderCurrentScreen()
    processButtonEvents()
    processBLECommands()

    if (dutCompleteSemaphore)
        → updateProgress, sendBLEData, drawBodePlot

    if (measurementCompleteSemaphore)
        → transitionToResults, exportCSV

    vTaskDelay(16ms) // ~60 FPS
}
```

---

## Module Architecture

### 1. Main Controller (`main.cpp`, 359 LOC)

**Global State**:
```cpp
// Data storage
ImpedancePoint baselineImpedanceData[MAX_DUT_COUNT][MAX_FREQUENCIES];     // 4×50
ImpedancePoint measurementImpedanceData[MAX_DUT_COUNT][MAX_FREQUENCIES];  // 4×50
int frequencyCount[MAX_DUT_COUNT];  // Counts per DUT

// Measurement control
bool baselineMeasurementDone = false;
bool finalMeasurementDone = false;
int numDUTs = 4;
uint8_t startFreqIndex = 0;
uint8_t endFreqIndex = 37;

// FreeRTOS synchronization
QueueHandle_t measurementQueue;  // 20 items
SemaphoreHandle_t dutCompleteSemaphore;
SemaphoreHandle_t measurementCompleteSemaphore;
```

**Key Functions**:
- `setup()` - Hardware initialization, task creation (main.cpp:309-353)
- `taskDataProcessor()` - Impedance calculation loop (main.cpp:87-135)
- `taskGUI()` - GUI update loop (main.cpp:137-262)

---

### 2. UART Communication (`UART_Functions.cpp`, 435 LOC)

**Circular Buffer Implementation**:
```cpp
#define UART_BUFFER_SIZE 512
volatile uint8_t uartBuffer[UART_BUFFER_SIZE];
volatile uint16_t uartBufferHead = 0;
volatile uint16_t uartBufferTail = 0;
SemaphoreHandle_t uartDataSemaphore;
```

**Interrupt Service Routine**:
```cpp
void ARDUINO_ISR_ATTR uartISR() {
    while (Serial1.available()) {
        uint8_t byte = Serial1.read();
        uartBuffer[uartBufferHead] = byte;
        uartBufferHead = (uartBufferHead + 1) % UART_BUFFER_SIZE;
    }
    xSemaphoreGiveFromISR(uartDataSemaphore, NULL);
}
```

**Command Sending**:
- `sendStartCommand(num_duts, startIdx, endIdx)` - 15-byte packet
- `sendStopCommand()` - Stop measurement
- `sendSetPGAGainCommand(gain)` - Adjust amplifier gain
- `sendSetTIAGainCommand(isLowGain)` - Select TIA range

**Packet Parsing**:
- `processUARTByte(byte)` - State machine processor
- Validates 0xAA start and 0x55 end delimiters
- Handles three packet types: DUT_START (0x10), FREQ_DATA (0x11), DUT_END (0x12)

---

### 3. Bluetooth LE (`BLE_Functions.cpp`, 376 LOC)

**Service Configuration**:
```cpp
Service UUID:  12345678-1234-5678-1234-56789abcdef0
Device Name:   BioPal-ESP32
MTU:           517 bytes (large packets for data)

Characteristics:
  - RX (write):  12345678-1234-5678-1234-56789abcdef1  (Client → ESP32)
  - TX (notify): 12345678-1234-5678-1234-56789abcdef2  (ESP32 → Client)
```

**Command Protocol**:
```
Incoming (from mobile app):
  "BASELINE_START[,num_duts[,start,end]]"
  "MEAS_START"
  "STOP"

Outgoing (to mobile app):
  "STATUS:ready"
  "STATUS:Measuring:N"
  "DUT_START:N"
  "DATA:{JSON}"  // ImpedancePoint array as JSON
  "DUT_END:N"
  "Measurement Complete"
  "ERROR:message"
```

**Key Functions**:
- `initBLE()` - Setup GATT server, characteristics
- `sendBLEStatus(message)` - Send status string
- `sendBLEImpedanceData(dutNum, dataArray, count)` - JSON serialization
- `onBLEWrite(value)` - Parse incoming commands

---

### 4. Calibration Engine (`calibration.cpp`, 963 LOC)

**Calibration Modes**:
```cpp
enum CalibrationMode {
    CALIBRATION_MODE_LOOKUP,        // CSV lookup table (default)
    CALIBRATION_MODE_FORMULA,       // Quadratic formula fit
    CALIBRATION_MODE_SEPARATE_FILES // Voltage + TIA + PGA separate files
};
```

**Data Structures**:
```cpp
struct FreqCalibrationData {
    float frequency_hz;
    uint8_t tia_mode;        // 0=high (7500Ω), 1=low (37.5Ω)
    uint8_t pga_gain;        // 0-7 (maps to 1,2,5,10,20,50,100,200)
    float z_mag_gain;        // Magnitude correction factor
    float phase_offset;      // Phase correction (degrees)
};

struct CalibrationCoefficients {
    float m0, m1, m2;        // Magnitude: Z_cal = Z_raw / (m0 + m1*f + m2*f²)
    float a1, a2;            // Phase: φ_cal = φ_raw - (a1*f + a2*f²)
    float r_squared_mag;     // Fit quality
    float r_squared_phase;
    bool valid;
};
```

**Calibration Files**:
```
/data/calibration.csv
    Format: frequency_hz, tia_mode, pga_gain, z_mag_gain, unused, phase_offset
    Example: 1, 0, 2, 1.0234, 0.0, -2.34

/data/voltage.csv
    Format: frequency_hz, gain_factor, phase_offset

/data/tia_high.csv, /data/tia_low.csv
    Format: frequency_hz, gain_factor, phase_offset

/data/pga_1.csv through /data/pga_200.csv
    Format: frequency_hz, gain_factor, phase_offset

/data/ps_trace.csv (NEW - PS Trace calibration)
    Format: freq_hz, mag_ratio, phase_offset
    Example: 1, 1.0234, -2.34
    Purpose: Final calibration step to match PalmSens reference exactly
```

**Two-Step Calibration Process**:
```cpp
ImpedancePoint calibrate(ImpedancePoint raw) {
    // STEP 1: Apply existing calibration (one of three modes)

    // Mode 1: Lookup table interpolation
    FreqCalibrationData cal = getCalibrationPoint(freq, tia, pga);
    calibrated.magnitude = raw.magnitude / cal.z_mag_gain;
    calibrated.phase = raw.phase - cal.phase_offset;

    // Mode 2: Formula-based
    CalibrationCoefficients coef = getCoefficients(tia, pga);
    float mag_correction = coef.m0 + coef.m1*freq + coef.m2*freq*freq;
    calibrated.magnitude = raw.magnitude / mag_correction;

    // Mode 3: Separate files
    float v_gain = getVoltageGain(freq);
    float tia_gain = getTIAGain(freq, tia_mode);
    float pga_gain = getPGAGain(freq, pga_value);
    calibrated.magnitude = raw.magnitude / (v_gain * tia_gain * pga_gain);

    // STEP 2: Apply PS Trace calibration (final refinement)
    applyPSTraceCalibration(calibrated);
    // calibrated.magnitude *= mag_ratio
    // calibrated.phase += phase_offset
}
```

**Calibration Flow**:
```
Raw Measurement (STM32)
    ↓
Existing Calibration (calibration.csv, formula, or separate files)
    ↓ Z_intermediate
PS Trace Calibration (ps_trace.csv)
    Z_final.magnitude = Z_intermediate.magnitude × mag_ratio
    Z_final.phase = Z_intermediate.phase + phase_offset
    ↓
Final Calibrated Impedance (matches PalmSens exactly)
```

**Key Functions**:
- `loadCalibrationData()` - Mount LittleFS, parse CSV files, load PS Trace
- `getCalibrationPoint(freq, tia, pga)` - Lookup with linear interpolation
- `calibrate(ImpedancePoint)` - Apply corrections (two-step process)
- `calculateCoefficients()` - Fit quadratic formula to data
- `loadPSTraceCalibration()` - Load PS Trace calibration file
- `applyPSTraceCalibration(point)` - Apply final PS Trace correction

---

### 5. GUI State Machine (`gui_state.cpp`, 373 LOC)

**State Enumeration**:
```cpp
enum GUIState {
    GUI_SPLASH,              // Logo screen (2s)
    GUI_HOME,                // Main menu
    GUI_SETTINGS,            // Configuration screen
    GUI_FREQ_OVERRIDE,       // Custom frequency range
    GUI_BASELINE_PROGRESS,   // Real-time baseline measurement
    GUI_BASELINE_COMPLETE,   // Baseline done, ready for final
    GUI_FINAL_PROGRESS,      // Real-time final measurement
    GUI_RESULTS              // Completed results display
};
```

**State Management**:
```cpp
struct GUISettings {
    uint8_t numDUTs;          // 1-4
    bool autoCalibrate;       // Auto-adjust gains
    uint8_t startFreqIndex;   // Custom frequency range start
    uint8_t endFreqIndex;     // Custom frequency range end
    CalibrationMode calMode;  // Calibration algorithm
    bool showRawData;         // Display uncalibrated data
};

extern GUIState currentGUIState;
extern GUISettings guiSettings;
```

**Key Functions**:
- `setGUIState(newState)` - Transition with validation
- `handleGUIInput(buttonEvent)` - State-specific input handling
- `saveGUISettings()` - Persist to LittleFS
- `loadGUISettings()` - Restore from flash

---

### 6. Display Rendering (`gui_screens.cpp`, 465 LOC)

**TFT Configuration**:
```cpp
#define TFT_WIDTH 320
#define TFT_HEIGHT 240
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);  // Double buffering
```

**Screen Rendering Functions**:
- `drawSplashScreen()` - Logo + version
- `drawHomeScreen()` - DUT selector, start button, settings button
- `drawSettingsScreen()` - Configuration options
- `drawProgressScreen(dutStatus[], progress)` - Real-time progress with DUT grid
- `drawResultsScreen()` - Summary with Bode plots
- `drawBodePlot(data[], count, x, y, w, h)` - Embedded impedance plots

**Helper Graphics**:
- `drawButton(x, y, w, h, label, pressed)` - Styled button
- `drawProgressBar(x, y, w, h, percent)` - Progress indicator
- `drawDUTStatusGrid(dutStatus[])` - 4-cell status grid (measuring/complete)

**Rendering Strategy**:
```cpp
void renderCurrentScreen() {
    sprite.fillSprite(TFT_BLACK);  // Clear sprite buffer

    switch(currentGUIState) {
        case GUI_HOME: drawHomeScreen(); break;
        case GUI_BASELINE_PROGRESS: drawProgressScreen(); break;
        ...
    }

    sprite.pushSprite(0, 0);  // Blit to screen (flicker-free)
}
```

---

### 7. Bode Plot (`bode_plot.cpp`, 290 LOC)

**Logarithmic Scaling**:
```cpp
// Frequency axis: Log scale
int freqToX(float freq_hz, float minFreq, float maxFreq, int plotWidth) {
    float logFreq = log10(freq_hz);
    float logMin = log10(minFreq);
    float logMax = log10(maxFreq);
    return (logFreq - logMin) / (logMax - logMin) * plotWidth;
}

// Magnitude axis: Log scale
int magToY(float mag_ohms, float minMag, float maxMag, int plotHeight) {
    float logMag = log10(mag_ohms);
    float logMin = log10(minMag);
    float logMax = log10(maxMag);
    return plotHeight - (logMag - logMin) / (logMax - logMin) * plotHeight;
}

// Phase axis: Linear scale
int phaseToY(float phase_deg, float minPhase, float maxPhase, int plotHeight) {
    return plotHeight - (phase_deg - minPhase) / (maxPhase - minPhase) * plotHeight;
}
```

**Plot Drawing**:
```cpp
void drawBodePlot(ImpedancePoint data[], int count, int x, int y, int w, int h) {
    // Auto-scale to data range
    float minFreq, maxFreq, minMag, maxMag, minPhase, maxPhase;
    calculateDataRange(data, count, &minFreq, &maxFreq, ...);

    // Draw axes and grid
    drawLogGrid(x, y, w, h, minFreq, maxFreq);

    // Draw magnitude curve (solid line, cyan)
    for (int i = 1; i < count; i++) {
        drawLine(freqToX(f1), magToY(mag1), freqToX(f2), magToY(mag2), TFT_CYAN);
    }

    // Draw phase curve (dashed line, yellow)
    for (int i = 1; i < count; i++) {
        drawDashedLine(freqToX(f1), phaseToY(phase1), ..., TFT_YELLOW);
    }
}
```

---

### 8. Button Handler (`button_handler.cpp`, 170 LOC)

**GPIO Configuration**:
```cpp
#define BTN_UP_PIN 16
#define BTN_DOWN_PIN 8
#define BTN_LEFT_PIN 14
#define BTN_RIGHT_PIN 17
#define BTN_SELECT_PIN 9
#define ENCODER_A_PIN 6
#define ENCODER_B_PIN 7
```

**Event Structure**:
```cpp
struct ButtonEvent {
    ButtonType button;  // UP, DOWN, LEFT, RIGHT, SELECT, ENCODER_CW, ENCODER_CCW
    uint32_t timestamp;
};

QueueHandle_t buttonEventQueue;  // 10 events
```

**Interrupt Service Routine**:
```cpp
void ARDUINO_ISR_ATTR buttonISR() {
    if (millis() - lastDebounceTime < DEBOUNCE_DELAY_MS)
        return;  // Debounce: 250ms

    ButtonEvent event = {button, millis()};
    xQueueSendFromISR(buttonEventQueue, &event, NULL);
    lastDebounceTime = millis();
}
```

**Rotary Encoder Decoding**:
```cpp
void ARDUINO_ISR_ATTR encoderISR() {
    bool a = digitalRead(ENCODER_A_PIN);
    bool b = digitalRead(ENCODER_B_PIN);

    // Gray code decoding (2 pulses per detent)
    if (a && !lastA) {
        ButtonEvent event = {b ? ENCODER_CW : ENCODER_CCW, millis()};
        xQueueSendFromISR(buttonEventQueue, &event, NULL);
    }
    lastA = a;
}
```

---

### 9. Impedance Calculation (`impedance_calc.cpp`, 31 LOC)

**Simple Calculation**:
```cpp
ImpedancePoint calcImpedance(MeasurementPoint mp) {
    ImpedancePoint imp;

    imp.freq_hz = mp.freq_hz;
    imp.magnitude = mp.V_magnitude / mp.I_magnitude;  // Ohms = V / A
    imp.phase = mp.V_phase - mp.I_phase;              // Phase difference
    imp.pga_gain = mp.pga_gain;
    imp.tia_gain = mp.tia_gain;
    imp.valid = mp.valid && (mp.I_magnitude > 0.0f);

    return imp;
}
```

---

### 10. Data Export (`csv_export.cpp`, 25 LOC)

**CSV Format**:
```cpp
void exportCSV(ImpedancePoint data[][MAX_FREQUENCIES], int dutCounts[]) {
    Serial.println("DUT,Frequency_Hz,Magnitude_Ohms,Phase_Deg");

    for (int dut = 0; dut < numDUTs; dut++) {
        for (int freq = 0; freq < dutCounts[dut]; freq++) {
            Serial.printf("%d,%.1f,%.4f,%.2f\n",
                dut + 1,
                data[dut][freq].freq_hz,
                data[dut][freq].magnitude,
                data[dut][freq].phase);
        }
    }
}
```

---

### 11. Serial Commands (`serial_commands.cpp`, 75 LOC)

**Command Interface**:
```
Commands:
  start [num_duts]   - Start measurement (default 4)
  stop               - Stop measurement
  help               - Show help

Example:
  > start 2
  Starting measurement with 2 DUTs...
  > stop
  Measurement stopped.
```

---

## Key Design Patterns

### 1. Producer-Consumer with Queue
```
UART Reader (producer) → Queue → Data Processor (consumer)
- Decouples reception from processing
- Prevents blocking during calibration
- Allows burst reception
```

### 2. Interrupt-Driven I/O
```
UART ISR → Circular Buffer → Semaphore → Task
- No polling overhead
- Fast ISR (just buffer byte)
- Safe processing in task context
```

### 3. Double Buffering
```
Sprite (off-screen) → Render → pushSprite() → Screen
- Eliminates flicker
- Complex graphics without tearing
- 60 FPS refresh rate
```

### 4. State Machine Pattern
```
GUI State Machine: Explicit states with transition validation
UART State Machine: Packet parsing with error recovery
- Clear logic flow
- Easy to debug
- Predictable behavior
```

### 5. Modular Calibration
```
Interface: calibrate(ImpedancePoint) → ImpedancePoint
Implementations: Lookup, Formula, Separate Files
- Swappable algorithms
- A/B testing friendly
- Incremental improvements
```

---

## Memory Usage

### RAM Allocation
```
Impedance Data Arrays:
  baselineImpedanceData:    4 DUTs × 50 freq × 24 bytes = 4.8 KB
  measurementImpedanceData: 4 DUTs × 50 freq × 24 bytes = 4.8 KB

FreeRTOS:
  Task stacks:              4KB + 8KB + 4KB = 16 KB
  Queues/semaphores:        ~1 KB

Display:
  TFT sprite buffer:        320 × 240 × 2 bytes = 150 KB (!)

Total:                      ~177 KB / 512 KB available (35%)
```

### Flash Usage
```
Program code:               ~400 KB / 4 MB (10%)
Calibration data files:     ~50 KB
LittleFS filesystem:        ~128 KB partition
```

---

## Error Handling

### UART Errors
- **Invalid packet**: Log error, reset state machine
- **Buffer overflow**: Drop oldest data (rare with 512B buffer)
- **Timeout**: 10-second timeout in taskUARTReader, reset state

### Calibration Errors
- **File not found**: Fall back to no calibration (Z_cal = Z_raw)
- **Parse error**: Skip malformed line, continue
- **Out of range**: Return nearest calibration point (extrapolation)

### GUI Errors
- **Invalid state transition**: Log error, stay in current state
- **BLE disconnect**: Continue operation, disable wireless features
- **Display init failure**: Halt with error message on serial

---

## Performance Characteristics

### Processing Speed
- **Impedance Calculation**: <1ms per frequency point
- **Calibration Lookup**: 2-5ms (CSV interpolation)
- **BLE Transmission**: 50-100ms per DUT (JSON serialization)
- **Display Update**: 16-33ms (30-60 FPS)

### Responsiveness
- **Button Latency**: <50ms (ISR → queue → GUI task)
- **UART Reception**: <10ms (ISR → buffer → parser)
- **Command Response**: <100ms (BLE or Serial)

### Throughput
- **UART**: 3600 baud = 450 bytes/sec theoretical, ~300 bytes/sec actual
- **BLE**: ~10 KB/sec (limited by notification rate)
- **USB Serial**: ~11.5 KB/sec (115200 baud)

---

## Summary

BioPal ESP32 is a well-architected embedded application featuring:
- **Multi-tasking**: FreeRTOS enables concurrent UART, processing, and GUI
- **Modular Design**: Each subsystem is self-contained with clear interfaces
- **Flexible Calibration**: Multiple modes support different accuracy/speed trade-offs
- **Responsive UI**: Interrupt-driven input and double-buffered display
- **Wireless Control**: BLE enables headless operation from mobile apps
- **Data Export**: CSV format for analysis in external tools

The system demonstrates professional embedded software practices including proper use of interrupts, queues, semaphores, state machines, and modular architecture.
