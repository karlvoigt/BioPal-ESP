# BioPal ESP32 Firmware Flow Diagram

## Detailed Program Flow

This diagram shows the complete execution flow with all three FreeRTOS tasks and their interactions.

```mermaid
graph TB
    Start([Power On]) --> Setup[Setup Function]

    Setup --> Init1[Initialize Serial<br/>115200 baud USB-CDC]
    Init1 --> Init2[Initialize TFT<br/>Sprite Buffer]
    Init2 --> Init3[Mount LittleFS<br/>Load Calibration Data]
    Init3 --> Init4[Create Measurement Queue<br/>20 items]
    Init4 --> Init5[Initialize UART to STM32<br/>3600 baud GPIO2/3]
    Init5 --> Init6[Initialize BLE Server<br/>UUID: 12345678...]
    Init6 --> CreateTasks[Create FreeRTOS Tasks]

    CreateTasks --> TaskUART[Task: UART Reader<br/>Priority 2, 4KB Stack]
    CreateTasks --> TaskDP[Task: Data Processor<br/>Priority 2, 8KB Stack]
    CreateTasks --> TaskGUI[Task: GUI<br/>Priority 1, 4KB Stack]

    TaskUART --> UART1{Wait for<br/>UART Data}
    UART1 -->|Semaphore| UART2[Read from<br/>Circular Buffer]
    UART2 --> UART3{State Machine}

    UART3 -->|WAITING_START| UART4[Wait for 0xAA]
    UART4 -->|Found| UART5[Read Packet Type]

    UART5 -->|0x10: DUT_START| UART6[Parse DUT Number<br/>& Frequency Count]
    UART6 --> UART7[Set Current DUT<br/>Reset Freq Counter]
    UART7 --> UART1

    UART5 -->|0x11: FREQ_DATA| UART8[Parse 26-byte Packet:<br/>- Frequency<br/>- V magnitude/phase<br/>- I magnitude/phase<br/>- PGA/TIA gains<br/>- Valid flag]
    UART8 --> UART9[Queue MeasurementPoint<br/>to measurementQueue]
    UART9 --> UART10[Increment Frequency Counter]
    UART10 --> UART1

    UART5 -->|0x12: DUT_END| UART11[Signal<br/>dutCompleteSemaphore]
    UART11 --> UART1

    TaskDP --> DP1{Wait for Queue<br/>Item}
    DP1 -->|Item Available| DP2[Dequeue<br/>MeasurementPoint]
    DP2 --> DP3[Calculate Impedance:<br/>Z = V_mag / I_mag<br/>Phase = V_phase - I_phase]
    DP3 --> DP4[Apply Calibration:<br/>- Lookup table<br/>- Formula<br/>- Separate files]
    DP4 --> DP5[Store in<br/>impedanceData array]
    DP5 --> DP6[Increment<br/>frequencyCount]
    DP6 --> DP1

    TaskGUI --> GUI1[Initialize<br/>GUI_SPLASH State]
    GUI1 --> GUI2{Current State?}

    GUI2 -->|SPLASH| GUI3[Display Logo<br/>Wait 2s]
    GUI3 --> GUI4[Transition to<br/>GUI_HOME]

    GUI2 -->|HOME| GUI5[Display Home Screen:<br/>- DUT Selector<br/>- START button<br/>- Settings button]
    GUI5 --> GUI6{User Input?}
    GUI6 -->|START| GUI7[Send UART START<br/>Command to STM32]
    GUI7 --> GUI8[Set State:<br/>BASELINE_PROGRESS]

    GUI2 -->|BASELINE_PROGRESS| GUI9[Display Progress:<br/>- DUT status grid<br/>- Progress bar<br/>- Current frequency]
    GUI9 --> GUI10{Check<br/>dutCompleteSemaphore}
    GUI10 -->|DUT Complete| GUI11[Update DUT Status<br/>Send BLE Data<br/>Draw Bode Plot]
    GUI11 --> GUI12{All DUTs<br/>Complete?}
    GUI12 -->|No| GUI9
    GUI12 -->|Yes| GUI13[Set State:<br/>BASELINE_COMPLETE]

    GUI2 -->|BASELINE_COMPLETE| GUI14[Display:<br/>Baseline Done<br/>Ready for Final]
    GUI14 --> GUI15{User Input?}
    GUI15 -->|Start Final| GUI16[Send UART START<br/>Set State: FINAL_PROGRESS]

    GUI2 -->|FINAL_PROGRESS| GUI17[Display Progress<br/>Similar to Baseline]
    GUI17 --> GUI18{Measurement<br/>Complete?}
    GUI18 -->|Yes| GUI19[Set State:<br/>GUI_RESULTS]

    GUI2 -->|RESULTS| GUI20[Display Results:<br/>- Bode plots<br/>- CSV data<br/>- Export button]
    GUI20 --> GUI21{User Input?}
    GUI21 -->|Home| GUI4

    GUI2 -->|Any State| GUI22[Process BLE Commands:<br/>BASELINE_START<br/>MEAS_START<br/>STOP]
    GUI22 --> GUI2

    GUI2 -->|Any State| GUI23[Handle Button Events:<br/>UP/DOWN/LEFT/RIGHT<br/>SELECT<br/>Rotary Encoder]
    GUI23 --> GUI2

    GUI6 -->|Settings| Settings[GUI_SETTINGS State]
    Settings --> GUI2

    style Start fill:#90EE90
    style TaskUART fill:#FFE4B5
    style TaskDP fill:#FFE4B5
    style TaskGUI fill:#FFE4B5
    style UART9 fill:#FFDAB9
    style DP4 fill:#FFDAB9
    style GUI11 fill:#98FB98
    style GUI20 fill:#98FB98
```

---

## Simplified Flow Diagram (For Reports)

This simplified horizontal diagram shows the overall system flow suitable for presentations and reports.

```mermaid
graph LR
    Start([System Start]) --> Init[Initialize<br/>Hardware & Load<br/>Calibration Data]
    Init --> Home[Display<br/>Home Screen<br/>Select DUTs]

    Home -->|User/BLE START| SendCmd[Send START<br/>Command to STM32<br/>via UART]

    SendCmd --> Loop{For Each DUT}

    Loop --> Receive[Receive Measurement<br/>Data from STM32]

    Receive --> Process[Calculate<br/>Impedance<br/>Z = V/I]

    Process --> Calibrate[Apply<br/>Calibration<br/>Corrections]

    Calibrate --> Display[Update Display<br/>Progress Bar]

    Display --> BLE[Transmit Results<br/>via BLE & Serial for debugging]

    BLE -->|More DUTs| Loop
    BLE -->|Complete| Results[Show Results<br/>Screen with<br/>Final Data]

    Results --> Home

    style Start fill:#90EE90
    style Init fill:#FFE4B5
    style Home fill:#87CEEB
    style SendCmd fill:#FFD700
    style Receive fill:#FFD700
    style Process fill:#FFDAB9
    style Calibrate fill:#FFDAB9
    style Display fill:#98FB98
    style BLE fill:#98FB98
    style Results fill:#90EE90
```

### Simplified Flow Explanation

1. **Initialization**: Setup hardware, load calibration data from flash
2. **Home Screen**: User selects number of DUTs (1-4) and presses START
3. **Send Command**: ESP32 sends START command to STM32 via UART (3600 baud)
4. **Data Reception Loop** (for each DUT):
   - Receive 38 frequency measurements from STM32
   - Each measurement contains voltage/current magnitude and phase
5. **Impedance Calculation**: Z = V_magnitude / I_magnitude
6. **Calibration**: Apply calibration corrections from lookup table or formula
7. **Display Update**: Show progress bar, DUT status, and real-time Bode plot
8. **BLE Transmission**: Send results to connected mobile/web app
9. **Results Screen**: Display final impedance data and Bode plots
10. **Export**: Save data as CSV via USB serial

**Total Time per Measurement**: ~42 seconds per DUT (controlled by STM32)

---

## FreeRTOS Task Architecture

The system uses three concurrent tasks running in parallel:

```mermaid
graph TB
    subgraph "Task: UART Reader (Priority 2)"
        T1[Wait for UART ISR Semaphore]
        T1 --> T2[Read Bytes from<br/>Circular Buffer]
        T2 --> T3[Parse Packets:<br/>DUT_START, FREQ_DATA, DUT_END]
        T3 --> T4[Queue MeasurementPoint]
        T4 --> T1
    end

    subgraph "Task: Data Processor (Priority 2)"
        D1[Wait for Queue Item]
        D1 --> D2[Calculate Impedance]
        D2 --> D3[Apply Calibration]
        D3 --> D4[Store in Array]
        D4 --> D1
    end

    subgraph "Task: GUI (Priority 1)"
        G1[Render Current Screen]
        G1 --> G2[Process Button Events]
        G2 --> G3[Handle BLE Commands]
        G3 --> G4[Update Display]
        G4 --> G1
    end

    T4 -.Queue.-> D1
    D4 -.Data Ready.-> G1

    style T1 fill:#FFE4B5
    style D1 fill:#FFDAB9
    style G1 fill:#98FB98
```

### Task Communication

- **UART ISR → UART Reader**: Semaphore signals data available in circular buffer
- **UART Reader → Data Processor**: Queue containing MeasurementPoint structures
- **Data Processor → GUI**: Shared memory arrays (impedanceData) with atomic counters
- **GUI ← User/BLE**: Button events and BLE command strings trigger state changes

---

## State Machine Diagram

### GUI State Machine

```mermaid
stateDiagram-v2
    [*] --> SPLASH
    SPLASH --> HOME : 2 seconds

    HOME --> SETTINGS : Settings button
    HOME --> FREQ_OVERRIDE : Freq button
    HOME --> BASELINE_PROGRESS : START button

    SETTINGS --> HOME : Back
    FREQ_OVERRIDE --> HOME : Back

    BASELINE_PROGRESS --> BASELINE_PROGRESS : Receiving data
    BASELINE_PROGRESS --> BASELINE_COMPLETE : All DUTs done
    BASELINE_PROGRESS --> HOME : STOP command

    BASELINE_COMPLETE --> FINAL_PROGRESS : Start Final
    BASELINE_COMPLETE --> HOME : Back

    FINAL_PROGRESS --> FINAL_PROGRESS : Receiving data
    FINAL_PROGRESS --> RESULTS : All DUTs done
    FINAL_PROGRESS --> HOME : STOP command

    RESULTS --> HOME : Home button
```

### UART Receiver State Machine

```mermaid
stateDiagram-v2
    [*] --> WAITING_START

    WAITING_START --> READING_PACKET_TYPE : 0xAA received
    READING_PACKET_TYPE --> READING_DUT_START : Packet type 0x10
    READING_PACKET_TYPE --> READING_FREQUENCY : Packet type 0x11
    READING_PACKET_TYPE --> READING_DUT_END : Packet type 0x12

    READING_DUT_START --> VALIDATING_END : Parse DUT info
    READING_FREQUENCY --> VALIDATING_END : Parse 26 bytes
    READING_DUT_END --> VALIDATING_END : Parse DUT number

    VALIDATING_END --> WAITING_START : 0x55 received, valid
    VALIDATING_END --> WAITING_START : Invalid packet
```

---

## Data Flow Diagram

```
┌──────────────────────────────────────────────────────────────┐
│                         STM32 Board                          │
│     (Generates sine waves, measures V & I via ADC/FFT)       │
└────────────────────┬─────────────────────────────────────────┘
                     │ UART @ 3600 baud
                     │ Binary packets
                     ▼
┌──────────────────────────────────────────────────────────────┐
│                      ESP32-C6 Board                          │
│  ┌────────────────────────────────────────────────────────┐  │
│  │ UART ISR → Circular Buffer (512 bytes)                 │  │
│  └────────────────────┬───────────────────────────────────┘  │
│                       │ Semaphore                             │
│                       ▼                                       │
│  ┌────────────────────────────────────────────────────────┐  │
│  │ Task: UART Reader (Parse packets)                      │  │
│  │  - DUT_START: Set current DUT                          │  │
│  │  - FREQ_DATA: Extract V, I, phase, gains              │  │
│  │  - DUT_END: Signal completion                          │  │
│  └────────────────────┬───────────────────────────────────┘  │
│                       │ Queue (MeasurementPoint)              │
│                       ▼                                       │
│  ┌────────────────────────────────────────────────────────┐  │
│  │ Task: Data Processor                                   │  │
│  │  1. Z = V_magnitude / I_magnitude                      │  │
│  │  2. Load calibration coefficients                      │  │
│  │  3. Calibrated_Z = Z_raw / calibration_gain           │  │
│  │  4. Store in impedanceData[DUT][freq]                 │  │
│  └────────────────────┬───────────────────────────────────┘  │
│                       │ Shared Array                          │
│                       ▼                                       │
│  ┌────────────────────────────────────────────────────────┐  │
│  │ Task: GUI                                              │  │
│  │  - Render progress screen & Bode plots                │  │
│  │  - Handle button/encoder input                        │  │
│  │  - Process BLE commands                               │  │
│  │  - Send BLE/Serial data                               │  │
│  └────────────────────┬───────────────────────────────────┘  │
│                       │                                       │
│                       ├─────> TFT Display (320×240)          │
│                       ├─────> BLE (Mobile App)               │
│                       └─────> USB Serial (CSV Export)        │
└──────────────────────────────────────────────────────────────┘
```

---

## Timing Summary

| Phase | Duration | Notes |
|-------|----------|-------|
| System Initialization | ~2 seconds | Load calibration, setup BLE/UART |
| Splash Screen | 2 seconds | Display logo |
| User Selection | Variable | Wait for user to select DUTs and press START |
| STM32 Measurement | ~42s per DUT | STM32 controls timing (1s settle + measurements) |
| Data Processing | <10ms per frequency | ESP32 calculation and calibration |
| BLE Transmission | ~2s per DUT | Send JSON data to mobile app |
| Display Update | 16-33ms | GUI refresh rate (30-60 FPS) |
| **Total for 4 DUTs** | **~3 minutes** | End-to-end measurement cycle |

---

## Key Design Features

1. **Non-blocking Architecture**: FreeRTOS tasks ensure responsive UI during measurements
2. **Interrupt-driven UART**: ISR fills buffer, task processes (no polling overhead)
3. **Queue-based Processing**: Loose coupling between UART reception and data processing
4. **Double-buffered Display**: Sprite system eliminates flicker
5. **Real-time Progress**: UI updates as each frequency point completes
6. **Wireless Control**: BLE commands allow headless operation from mobile app
7. **Flexible Calibration**: Multiple calibration modes support different accuracy requirements
