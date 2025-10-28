# Hardware Configuration

## Microcontroller

**ESP32-C6 DevKit**
- RISC-V single-core processor @ 160 MHz
- 512 KB SRAM, 4 MB Flash
- Integrated WiFi 6 + Bluetooth 5.3 LE
- 30 GPIO pins (multi-function)
- USB Serial JTAG (native USB-CDC support)
- Low power modes for battery operation

## Peripheral Configuration

### UART (Serial Communication)

#### UART0 - USB Serial (Debug & Data Export)
- **Interface**: USB-CDC (native USB)
- **Baud Rate**: 115200
- **Function**: Console output, CSV export, command interface
- **Implementation**: `Serial` object (Arduino)

#### UART1 - STM32 Communication
- **Configuration**: `UART_Functions.cpp:22-42`
- **Baud Rate**: 3600 (intentionally slow for reliability)
- **Pins**:
  - TX: GPIO 3
  - RX: GPIO 2
- **Function**: Binary protocol communication with STM32 measurement board
- **Buffer**: 512-byte circular buffer (interrupt-driven)

**UART1 Initialization**:
```cpp
Serial1.begin(3600, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
Serial1.onReceive(uartISR);  // Interrupt on receive
```

---

### SPI (TFT Display)

#### TFT Display Interface
- **Controller**: ILI9341 or compatible
- **Resolution**: 320×240 pixels, 16-bit color (65K colors)
- **Interface**: Hardware SPI
- **Library**: TFT_eSPI (customized)

**Pin Assignments** (from `TFT_eSPI/User_Setup.h`):
```
SCK  (Clock):     GPIO 18
MOSI (Data Out):  GPIO 23
MISO (Data In):   GPIO 19  (unused for TFT, but configured)
CS   (Chip Sel):  GPIO 5
DC   (Data/Cmd):  GPIO 27
RST  (Reset):     GPIO 33
BL   (Backlight): GPIO 32  (PWM control for brightness)
```

**SPI Configuration**:
- **Frequency**: 40 MHz (high speed for fast refresh)
- **Mode**: SPI_MODE0
- **Bit Order**: MSB first

**Display Features**:
- Double buffering via TFT_eSprite (eliminates flicker)
- Hardware acceleration for primitives (lines, rectangles, circles)
- Custom fonts and UTF-8 support
- DMA transfers for maximum performance

---

### GPIO (Digital I/O)

#### Input Pins (Buttons & Encoder)

**Button Configuration** (`pinDefs.h`, `button_handler.cpp:12-20`):
```cpp
#define BTN_UP_PIN      16  // Up navigation
#define BTN_DOWN_PIN     8  // Down navigation
#define BTN_LEFT_PIN    14  // Left navigation
#define BTN_RIGHT_PIN   17  // Right navigation
#define BTN_SELECT_PIN   9  // Select/confirm

#define ENCODER_A_PIN    6  // Rotary encoder phase A
#define ENCODER_B_PIN    7  // Rotary encoder phase B
```

**Input Configuration**:
- **Pull-up**: Internal pull-up enabled on all buttons
- **Interrupt**: GPIO interrupt on falling edge
- **Debounce**: 250ms software debounce in ISR

**Rotary Encoder**:
- **Type**: Mechanical encoder with 2 pulses per detent
- **Decoding**: Gray code (quadrature) decoding in ISR
- **Resolution**: 1 detent = 1 encoder event (CW or CCW)

#### Power Control

**5V Power Enable** (for STM32 board):
```
EN_5V_PIN: GPIO (specific pin TBD in pinDefs.h)
Function: Enable/disable 5V power to STM32 board
```

---

### LittleFS (Filesystem)

#### Flash Filesystem
- **Type**: LittleFS (lightweight filesystem for embedded)
- **Partition**: 128 KB dedicated partition in flash
- **Mount Point**: `/littlefs` (default)

**Files Stored**:
- `/calibration.csv` - Main calibration lookup table
- `/voltage.csv` - Voltage measurement calibration
- `/tia_high.csv` - TIA high-gain (7500Ω) calibration
- `/tia_low.csv` - TIA low-gain (37.5Ω) calibration
- `/pga_1.csv` through `/pga_200.csv` - PGA gain calibration (8 files)
- `/gui_settings.dat` - GUI settings (binary format)

**File Operations**:
```cpp
#include <LittleFS.h>

void setup() {
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        return;
    }

    File file = LittleFS.open("/calibration.csv", "r");
    // Parse CSV...
    file.close();
}
```

**Filesystem Management**:
- **Wear Leveling**: Automatic (built into LittleFS)
- **Power-Loss Protection**: Safe for sudden power loss
- **Max File Size**: Limited by available partition space (~100 KB usable)

---

### Bluetooth LE

#### BLE Configuration
**Controller**: ESP32-C6 integrated Bluetooth 5.3 LE radio

**GATT Server Setup** (`BLE_Functions.cpp:48-123`):
```
Device Name:        BioPal-ESP32
Service UUID:       12345678-1234-5678-1234-56789abcdef0

Characteristics:
  RX (write):       12345678-1234-5678-1234-56789abcdef1
    - Client writes commands to ESP32
    - Properties: WRITE
    - Max Length: 256 bytes

  TX (notify):      12345678-1234-5678-1234-56789abcdef2
    - ESP32 sends data to client
    - Properties: NOTIFY
    - Max Length: 512 bytes (MTU dependent)
```

**Advertising**:
- **Interval**: 20-40ms (fast discovery)
- **TX Power**: 0 dBm (default)
- **Connectable**: Yes
- **Timeout**: None (always discoverable when not connected)

**Connection Parameters**:
- **MTU**: 517 bytes (negotiated, allows large packets)
- **Connection Interval**: 15-30ms
- **Slave Latency**: 0 (low latency for real-time data)
- **Supervision Timeout**: 4 seconds

**Security**:
- **Bonding**: Disabled (open connection)
- **Encryption**: Optional (can be enabled for secure deployments)

---

## Pin Assignment Table

| Pin | Function | Direction | Notes |
|-----|----------|-----------|-------|
| GPIO 2 | UART1 RX | Input | STM32 TX → ESP32 RX |
| GPIO 3 | UART1 TX | Output | ESP32 TX → STM32 RX |
| GPIO 5 | TFT CS | Output | Chip select |
| GPIO 6 | Encoder A | Input | Rotary encoder phase A |
| GPIO 7 | Encoder B | Input | Rotary encoder phase B |
| GPIO 8 | Button DOWN | Input | Pull-up, interrupt |
| GPIO 9 | Button SELECT | Input | Pull-up, interrupt |
| GPIO 14 | Button LEFT | Input | Pull-up, interrupt |
| GPIO 16 | Button UP | Input | Pull-up, interrupt |
| GPIO 17 | Button RIGHT | Input | Pull-up, interrupt |
| GPIO 18 | SPI SCK | Output | TFT clock |
| GPIO 19 | SPI MISO | Input | (Unused for TFT) |
| GPIO 23 | SPI MOSI | Output | TFT data |
| GPIO 27 | TFT DC | Output | Data/command select |
| GPIO 32 | TFT BL | Output | Backlight PWM |
| GPIO 33 | TFT RST | Output | Reset |

**Unused Pins**: Available for future expansion (sensors, SD card, etc.)

---

## Hardware Block Diagram

```
┌────────────────────────────────────────────────────────────────┐
│                       ESP32-C6 DevKit                          │
│                                                                │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │  RISC-V Core @ 160 MHz                                   │ │
│  │  512 KB SRAM, 4 MB Flash                                 │ │
│  └──────────────────────────────────────────────────────────┘ │
│                                                                │
│  ┌────────────┐      ┌────────────┐      ┌────────────┐      │
│  │ UART1      │      │ SPI        │      │ GPIO       │      │
│  │ 3600 baud  │      │ 40 MHz     │      │ Interrupts │      │
│  │ GPIO 2/3   │      │ GPIO 18/23 │      │ GPIO 6-17  │      │
│  └──────┬─────┘      └──────┬─────┘      └──────┬─────┘      │
│         │                   │                   │             │
│  ┌──────▼────────┐   ┌──────▼─────┐     ┌──────▼─────┐      │
│  │ BLE Radio     │   │ USB-CDC    │     │ LittleFS   │      │
│  │ 5.3 LE        │   │ 115200     │     │ 128 KB     │      │
│  └──────┬────────┘   └──────┬─────┘     └──────┬─────┘      │
│         │                   │                   │             │
└─────────┼───────────────────┼───────────────────┼─────────────┘
          │                   │                   │
          ▼                   ▼                   ▼
  ┌───────────────┐   ┌──────────────┐   ┌──────────────┐
  │  Mobile App   │   │   USB Host   │   │  Flash Mem   │
  │  (BLE Client) │   │   (Serial)   │   │  (Calib)     │
  └───────────────┘   └──────────────┘   └──────────────┘

┌──────────────────────────────────────────────────────────────┐
│                       STM32 Board                            │
│                 (Impedance Measurement)                      │
└───────────────────────────▲──────────────────────────────────┘
                            │
                  UART @ 3600 baud (GPIO 2/3)

┌──────────────────────────────────────────────────────────────┐
│                       TFT Display                            │
│                 ILI9341 320×240 pixels                       │
└───────────────────────────▲──────────────────────────────────┘
                            │
                  SPI @ 40 MHz (GPIO 5/18/23/27/33)

┌──────────────────────────────────────────────────────────────┐
│                  Buttons & Rotary Encoder                    │
│         UP/DOWN/LEFT/RIGHT/SELECT + Encoder A/B              │
└───────────────────────────▲──────────────────────────────────┘
                            │
                  GPIO Interrupts (GPIO 6-17)
```

---

## Display Specifications

### TFT Screen
- **Size**: 2.8" diagonal (estimated)
- **Resolution**: 320×240 pixels (QVGA)
- **Color Depth**: 16-bit RGB565 (65,536 colors)
- **Controller**: ILI9341 or compatible
- **Interface**: 4-wire SPI
- **Refresh Rate**: ~60 Hz (limited by SPI bandwidth)
- **Backlight**: PWM-controlled LED backlight (GPIO 32)

### Display Capabilities
- **Graphics Primitives**: Lines, rectangles, circles, text
- **Fonts**: Multiple sizes (built-in + custom)
- **Images**: BMP, JPEG (via libraries)
- **Touch**: Not used in current implementation (button-only navigation)

### Double Buffering
```cpp
TFT_eSprite sprite = TFT_eSprite(&tft);
sprite.createSprite(320, 240);  // Allocate 150 KB in RAM

// Render to sprite (off-screen)
sprite.fillSprite(TFT_BLACK);
sprite.drawString("Hello", 10, 10, 4);
sprite.drawLine(0, 0, 319, 239, TFT_WHITE);

// Blit to screen (flicker-free)
sprite.pushSprite(0, 0);
```

**Memory Usage**: 320 × 240 × 2 bytes = 150 KB (largest RAM consumer)

---

## Input Devices

### Buttons
- **Type**: Tactile push buttons (momentary contact)
- **Configuration**: Active-low with internal pull-up
- **Debounce**: 250ms software debounce
- **Response Time**: <50ms (ISR to GUI task)

**Button Layout** (suggested physical arrangement):
```
        [UP]
[LEFT] [SELECT] [RIGHT]
       [DOWN]

   [ROTARY ENCODER]
```

### Rotary Encoder
- **Type**: Mechanical quadrature encoder
- **Resolution**: 2 pulses per detent (standard)
- **Phases**: A and B (90° phase shift)
- **Direction Detection**: Gray code decoding

**Encoder State Table**:
```
A B | Previous A | Direction
----|------------|----------
1 1 | 0          | CW
1 1 | 1          | CCW
0 0 | 1          | CW
0 0 | 0          | CCW
```

---

## Power Supply

### Operating Voltage
- **ESP32-C6**: 3.3V (internal regulator from USB 5V)
- **TFT Display**: 3.3V logic, 5V backlight (via level shifter or separate supply)
- **Buttons/Encoder**: 3.3V

### Power Consumption
- **ESP32-C6 Active**: ~80 mA (WiFi off, BLE idle)
- **ESP32-C6 WiFi TX**: ~200 mA (peak)
- **TFT Display**: ~50 mA (backlight on)
- **Total**: ~150 mA typical, 300 mA peak

### Power Sources
- **USB**: 5V from USB-C connector (native ESP32-C6 USB)
- **Battery** (optional): Li-ion battery via JST connector (if devkit supports)

---

## External Connections

### Connectors

#### USB-C
- **Function**: Power, programming, serial console
- **Standard**: USB 2.0 Full Speed (12 Mbps)
- **Interface**: Native USB-CDC (no UART-USB bridge chip)

#### UART1 Header (to STM32)
- **Type**: 4-pin header (VCC, GND, TX, RX)
- **Pinout**:
  - Pin 1: VCC (3.3V output from ESP32)
  - Pin 2: GND
  - Pin 3: TX (GPIO 3, ESP32 → STM32)
  - Pin 4: RX (GPIO 2, STM32 → ESP32)

**Cable**: 3-4 wire cable (TX/RX crossed, GND common)

#### TFT Display Header
- **Type**: 8-10 pin header or ribbon cable
- **Signals**: SPI (SCK, MOSI, CS), DC, RST, BL, VCC, GND

---

## Calibration Data Storage

### File Format
All calibration files use CSV format:
```csv
# Comment lines start with #
frequency_hz, tia_mode, pga_gain, z_mag_gain, unused, phase_offset
1, 0, 2, 1.0234, 0.0, -2.34
2, 0, 2, 1.0456, 0.0, -2.56
...
```

### File Sizes
```
calibration.csv:    ~20 KB  (38 freq × 16 gains × 3 fields × ~20 bytes/line)
voltage.csv:        ~2 KB   (38 frequencies)
tia_high.csv:       ~2 KB   (38 frequencies)
tia_low.csv:        ~2 KB   (38 frequencies)
pga_*.csv (×8):     ~16 KB  (8 files × 2 KB each)

Total:              ~44 KB / 128 KB partition (34% usage)
```

### Filesystem Layout
```
/littlefs/
├── calibration.csv       (Main lookup table)
├── voltage.csv           (Voltage calibration)
├── tia_high.csv          (TIA high-gain)
├── tia_low.csv           (TIA low-gain)
├── pga_1.csv             (PGA gain = 1)
├── pga_2.csv             (PGA gain = 2)
├── pga_5.csv             (PGA gain = 5)
├── pga_10.csv            (PGA gain = 10)
├── pga_20.csv            (PGA gain = 20)
├── pga_50.csv            (PGA gain = 50)
├── pga_100.csv           (PGA gain = 100)
├── pga_200.csv           (PGA gain = 200)
└── gui_settings.dat      (Binary settings, 128 bytes)
```

---

## Hardware Limitations

### Memory Constraints
- **RAM**: 512 KB total, ~350 KB available after system overhead
  - Display buffer: 150 KB (largest consumer)
  - Impedance data: ~10 KB
  - Task stacks: ~16 KB
  - Free for runtime: ~174 KB
- **Flash**: 4 MB total, ~3.5 MB available for user code/data

### Processing Constraints
- **CPU Speed**: 160 MHz (sufficient for real-time GUI, modest for FFT)
- **FPU**: None (software floating-point, slower than hardware FPU)
- **No DMA for Display**: SPI transfers block CPU (40 MHz helps)

### I/O Constraints
- **UART1 Speed**: 3600 baud limits throughput to ~300 bytes/sec
  - Full measurement (4 DUTs, 38 freq) = ~4 KB data
  - Transfer time: ~13 seconds (acceptable for 42s measurement)
- **BLE Throughput**: ~10 KB/sec (limited by notification rate)

---

## Hardware Setup Guide

### Connections

1. **Connect ESP32-C6 to STM32 via UART**:
   - ESP32 GPIO 3 (TX) → STM32 RX
   - ESP32 GPIO 2 (RX) → STM32 TX
   - GND → GND

2. **Connect TFT Display to ESP32**:
   - SPI pins (see table above)
   - Power: 3.3V and GND

3. **Connect Buttons and Encoder**:
   - All buttons to GPIO with internal pull-ups
   - Encoder phases A and B to GPIO 6/7

4. **Power via USB-C**:
   - Connect USB cable to computer or power adapter
   - No external power supply needed for typical use

### First Boot

1. Upload firmware via PlatformIO
2. Serial monitor shows initialization messages
3. Splash screen appears on TFT for 2 seconds
4. Home screen displays with DUT selector

### Troubleshooting

- **No display**: Check SPI connections, verify TFT_eSPI library configuration
- **UART errors**: Verify baud rate (3600), check TX/RX crossover
- **BLE not discoverable**: Check BLE initialization in serial console
- **Calibration file errors**: Re-upload `/data/` folder to LittleFS
