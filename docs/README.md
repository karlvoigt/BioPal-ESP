# BioPal ESP32 Firmware Documentation

## Overview

BioPal ESP32 is a sophisticated bioimpedance analyzer controller that manages multi-channel impedance measurements via communication with an STM32 measurement board. It provides real-time data processing, calibration, user interface control, and wireless connectivity.

**Target Hardware**: ESP32-C6 DevKit with TFT display
**Build System**: PlatformIO
**Architecture**: FreeRTOS task-based with event-driven communication

## Documentation Index

1. **[System Architecture](architecture.md)** - Complete system overview, modules, and FreeRTOS task architecture
2. **[Flow Diagram](flow-diagram.md)** - Visual flow diagrams (detailed and simplified) showing program execution
3. **[Hardware](hardware.md)** - ESP32-C6 configuration, TFT display, buttons, and pin assignments
4. **[Communication](communication.md)** - UART protocol with STM32, BLE interface, and USB serial commands

## Quick Start

The firmware operates as a multi-tasking system with three concurrent FreeRTOS tasks:

1. **UART Reader Task** - Receives and parses data packets from STM32
2. **Data Processor Task** - Calculates and calibrates impedance measurements
3. **GUI Task** - Manages display, user input, and BLE communication

### Main Functions

- **Impedance Measurement**: 1-4 DUTs, 1 Hz to 100 kHz (38 frequency points)
- **User Interface**: TFT touchscreen display with button controls
- **Wireless Control**: BLE interface for mobile/web app control
- **Data Export**: CSV format via USB serial
- **Calibration**: PalmSens reference-based calibration with multiple modes

## Key Files

- `src/main.cpp` - Entry point, task initialization, global state (359 lines)
- `src/UART_Functions.cpp` - STM32 communication driver (435 lines)
- `src/BLE_Functions.cpp` - Bluetooth LE wireless interface (376 lines)
- `src/calibration.cpp` - Impedance calibration engine (963 lines)
- `src/gui_state.cpp` - GUI state machine & settings (373 lines)
- `src/gui_screens.cpp` - TFT display rendering (465 lines)
- `src/bode_plot.cpp` - Bode plot visualization (290 lines)

## Features

- **Multi-DUT Support**: Measure 1-4 devices simultaneously
- **Wide Frequency Range**: 1 Hz to 100 kHz (38 logarithmic points)
- **Wireless Control**: BLE interface for mobile apps
- **Real-time Display**: Progress monitoring and Bode plot visualization
- **Calibration System**: Lookup table or formula-based calibration
- **Flexible Configuration**: Custom frequency ranges, gain settings
- **Data Export**: CSV and PS Trace format support

## Measurement Workflow

1. User selects number of DUTs (1-4) on home screen
2. Press START button (or BLE command)
3. ESP32 sends START command to STM32 via UART
4. STM32 performs measurements and streams data back
5. ESP32 processes, calibrates, and displays results in real-time
6. Completed data transmitted via BLE and USB serial
7. Results screen shows Bode plots and impedance data

## Communication Protocols

- **UART to STM32**: 3600 baud, GPIO2/3, binary protocol
- **BLE**: UUID 12345678-1234-5678-1234-56789abcdef0, MTU 517 bytes
- **USB Serial**: 115200 baud, USB-CDC, command interface and data export
