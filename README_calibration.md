# BioPal Calibration Tool

Tool for generating impedance calibration data by comparing PalmSens reference measurements with BioPal STM32 raw measurements.

## Overview

The calibration system has been redesigned to:
1. **Calculate impedance first** (Z = V/I), then **calibrate the impedance** (not voltage/current separately)
2. Use **PalmSens as reference** (ground truth)
3. Python tool talks **directly to STM32** via UART (bypasses ESP32)
4. Store **impedance magnitude gain** and **phase offset** per frequency/TIA/PGA combination

## Workflow

### 1. Measure Reference with PalmSens
- Connect DUT to PalmSens device
- Run EIS sweep (same frequency range as BioPal)
- Export data as CSV from PS Trace
- File will be UTF-16-LE encoded with columns: `freq / Hz, neg. Phase / °, Idc / uA, Z / Ohm, ...`

### 2. Run Calibration Tool
```bash
python calibration_tool.py <palmsens_csv> <output_calibration.csv>
```

**Example**:
```bash
python calibration_tool.py dut1_pbs.csv data/calibration.csv
```

### 3. Tool Process
1. Loads PalmSens reference data
2. Connects to STM32 via UART (you select from list)
3. Runs 2 measurement sweeps on same DUT
4. Averages the 2 sweeps (reduces noise)
5. For each frequency:
   - Calculates STM32 impedance: `Z_meas = V_mag / I_mag`
   - Gets PalmSens reference: `Z_ref`
   - Calculates: `z_mag_gain = Z_ref / Z_meas`
   - Calculates: `phase_offset = phase_ref - phase_meas`
6. Saves to calibration.csv
7. **Only updates measured frequency/gain combinations** (preserves existing data)

### 4. Upload Calibration to ESP32
- Copy `calibration.csv` to ESP32 filesystem at `/calibration.csv`
- Can use PlatformIO's filesystem uploader or serial upload tool

## How It Works

### ESP32 Calibration Flow

**Old Flow** (calibrate measurement):
```
MeasurementPoint → calibrate() → calcImpedance() → ImpedancePoint
```

**New Flow** (calibrate impedance):
```
MeasurementPoint → calcImpedance() → calibrate() → ImpedancePoint (calibrated)
```

### Key Changes

1. **ImpedancePoint** now includes `pga_gain` and `tia_gain` for calibration lookup
2. **calibrate()** changed from `calibrate(MeasurementPoint&)` to `calibrate(ImpedancePoint&)`
3. **CalibrationPoint** fields repurposed:
   - `voltage_gain` → now stores `z_mag_gain` (impedance magnitude calibration)
   - `current_gain` → unused (kept for backward compatibility)
   - `phase_offset` → same meaning

### Calibration CSV Format

Format unchanged, but field meanings changed:
```
freq,tia_mode,pga_gain,z_mag_gain,unused,phase_offset
```

**Columns**:
- `freq`: Frequency in Hz
- `tia_mode`: 0=high TIA, 1=low TIA
- `pga_gain`: 0-7 (maps to 1, 2, 5, 10, 20, 50, 100, 200)
- `z_mag_gain`: Impedance magnitude calibration gain (was voltage_gain)
- `unused`: Not used (was current_gain, kept for compatibility)
- `phase_offset`: Phase offset in degrees

**Example**:
```csv
# EIS Calibration Data
1,0,0,1.234567,1.0,5.23
2,0,0,1.235123,1.0,5.45
...
```

## STM32 UART Protocol

### Command: START Measurement
```
Packet: [0xAA][0x03][num_duts:4 bytes][0:4 bytes][0:4 bytes][0x55]
Response: [0xAA][0x03][0x01][0x55] (ACK)
```

### Data: Frequency Packet
```
[0xAA][0x11]
  [freq:4 bytes]
  [V_mag:4 bytes (scaled × 1000)]
  [V_phase:4 bytes (scaled × 100)]
  [I_mag:4 bytes (scaled × 1000)]
  [I_phase:4 bytes (scaled × 100)]
  [pga_gain:1 byte]
  [tia_gain:1 byte]
  [valid:1 byte]
[0x55]
```

Total: 26 bytes

## Requirements

```bash
pip install -r requirements.txt
```

Dependencies:
- `pyserial` - UART communication
- `numpy` - Phase averaging and calculations

## Tips

1. **Use same DUT for both measurements** (PalmSens and BioPal)
2. **Measure close in time** to avoid drift
3. **Keep environmental conditions stable** (temperature, humidity)
4. **Use same frequency range** for both instruments
5. **Run tool multiple times** to build up calibration for different gain settings
6. **Existing calibration data is preserved** - new measurements only update specific freq/gain combinations

## Troubleshooting

### No Serial Ports Found
- Check STM32 is connected via USB
- Check drivers installed (STLink, USB-to-Serial)
- Try different USB cable/port

### Timeout Waiting for Data
- Check STM32 is running firmware
- Check UART baud rate is 3600
- Check DUT is connected to STM32
- Try pressing STM32 reset button

### PalmSens CSV Not Recognized
- Check file is exported from PS Trace as CSV
- Should have header with "freq / Hz" and "Z / Ohm"
- File is UTF-16-LE encoded (tool handles this automatically)

### Calibration Not Applied on ESP32
- Check calibration.csv is uploaded to ESP32 filesystem at `/calibration.csv`
- Check ESP32 prints "Loaded calibration data for X frequencies" on startup
- Check frequency/gain combination exists in calibration file

## Example Session

```
$ python calibration_tool.py dut1_pbs.csv data/calibration.csv

============================================================
BioPal Calibration Tool
Generates calibration data using PalmSens reference
============================================================

Loading PalmSens reference: dut1_pbs.csv
✓ Loaded 38 reference points
  Freq range: 1.0 - 100000.0 Hz
  Z range: 14.7 - 64200.0 Ω

=== Available Serial Ports ===
1. /dev/cu.usbmodem14203 - STM32 STLink

Select STM32 port number: 1
✓ Connected to STM32 on /dev/cu.usbmodem14203 at 3600 baud

============================================================
SWEEP 1
============================================================
→ Sent START command (1 DUT)
← Received ACK

=== Receiving Sweep Data ===
← DUT 1 START (38 frequencies)
  1 Hz: V=0.334, I=0.005, φ=-40.23°, PGA=0, TIA=0
  2 Hz: V=0.267, I=0.004, φ=-45.12°, PGA=0, TIA=0
  ...
  100000 Hz: V=0.001, I=0.000, φ=-10.34°, PGA=5, TIA=0
← DUT 1 END
✓ Received 38 frequency points

[Sweep 2 similar output...]

=== Averaging Sweeps ===
✓ Averaged 38 points

=== Calculating Calibration ===
     1 Hz: Z_STM= 67234.2 Ω, Z_PS= 64200.0 Ω, gain=0.954823, Δφ=  5.23°
     2 Hz: Z_STM= 40512.8 Ω, Z_PS= 38760.0 Ω, gain=0.956723, Δφ=  5.45°
     ...
100000 Hz: Z_STM=    15.2 Ω, Z_PS=    14.7 Ω, gain=0.967105, Δφ= -1.23°

✓ Calculated calibration for 38 points

=== Saving Calibration ===
→ Loaded 150 existing calibration points
✓ Saved 188 calibration points to data/calibration.csv

============================================================
✓ Calibration Complete!
============================================================
```

## Notes

- The tool performs **incremental updates** - it won't overwrite your entire calibration file
- Each run adds/updates only the specific frequency/gain combinations measured
- Build up comprehensive calibration by running with different DUTs and frequency ranges
- STM32 auto-ranging will use different PGA/TIA gains depending on impedance magnitude
