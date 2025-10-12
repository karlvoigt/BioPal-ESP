#!/usr/bin/env python3
"""
STM32 CSV to Impedance Calculator
Reads STM32 voltage/current CSV data and applies ESP32 calibration formulas
to calculate impedance, saving results to Results/ESP.csv

Usage: python stm_to_impedance.py <input_csv>
"""

import sys
import csv
import math
import os

# Calibration constants (from ESP32 calibration.cpp)
V_GBW = 10.0  # Voltage stage Gain Bandwidth Product (MHz)
V_GAIN = 15  # INA331 Instrumentation Amplifier gain
I_GBW = 40.0  # Current stage Gain Bandwidth Product (MHz)
TLV_GAIN = 20.0  # TLV9061 OpAmp gain
TIA_GAINS = [7500.0, 37.6]  # [high=0, low=1]
PGA_CUTOFF = [10.0, 3.8, 1.8, 1.8, 1.3, 0.9, 0.38, 0.23]  # MHz
PGA_GAIN_VALUES = [1, 2, 5, 10, 20, 50, 100, 200]


def pga_enum_to_gain(pga_enum):
    """Convert PGA enum (0-7) to actual gain value"""
    if 0 <= pga_enum < len(PGA_GAIN_VALUES):
        return PGA_GAIN_VALUES[pga_enum]
    return 1


def get_calibration_point(freq_hz, tia_low, pga_gain_enum):
    """
    Calculate calibration point for given frequency and gain settings
    Returns: (v_gain, i_gain, phase_offset)
    """
    freq = float(freq_hz)

    # Calculate voltage gain and phase shift
    v_gain = TLV_GAIN * V_GAIN * (1 / math.sqrt(1 + pow(freq / (V_GBW / V_GAIN * 1e6), 2)))
    v_phase = -math.atan(freq / (V_GBW / V_GAIN * 1e6)) * 180.0 / math.pi

    # Calculate current gain and phase shift
    tia_gain = TIA_GAINS[tia_low]
    pga_gain = pga_enum_to_gain(pga_gain_enum)

    i_gain = (TLV_GAIN * tia_gain * (1 / math.sqrt(1 + pow(freq / (I_GBW / tia_gain * 1e6), 2))) *
              pga_gain * (1 / math.sqrt(1 + pow(freq / (PGA_CUTOFF[pga_gain_enum] * 1e6), 2))))

    i_phase = (-math.atan(freq / (I_GBW / tia_gain * 1e6)) * 180.0 / math.pi -
               math.atan(freq / (PGA_CUTOFF[pga_gain_enum] * 1e6)) * 180.0 / math.pi)

    phase_offset = v_phase - i_phase

    return v_gain, i_gain, phase_offset


def normalize_phase(phase):
    """Normalize phase to [-180, 180] range"""
    while phase > 180.0:
        phase -= 360.0
    while phase < -180.0:
        phase += 360.0
    return phase


def calibrate_and_calculate(v_mag, v_phase, i_mag, i_phase, freq_hz, pga_gain, tia_gain):
    """
    Apply calibration and calculate impedance
    Returns: (Z_magnitude, Z_phase)
    """
    # Get calibration point
    v_gain, i_gain, phase_offset = get_calibration_point(freq_hz, tia_gain, pga_gain)

    # Apply calibration (convert from scaled integers to actual values)
    v_calibrated = (v_mag / 1000.0) / v_gain  # Convert from mV to V and apply gain
    i_calibrated = (i_mag / 1000.0) / i_gain  # Convert from mV to V and apply gain

    # Calculate phase difference and normalize
    phase_diff = v_phase / 100.0 - i_phase / 100.0  # Convert from scaled int to degrees
    phase_diff = normalize_phase(phase_diff)

    # Apply phase calibration
    phase_calibrated = phase_diff - phase_offset
    phase_calibrated = normalize_phase(phase_calibrated)

    # Calculate impedance
    if i_calibrated == 0:
        return 0, 0

    Z_magnitude = v_calibrated / i_calibrated
    Z_phase = phase_calibrated

    return Z_magnitude, Z_phase


def parse_stm_csv(filepath):
    """
    Parse STM32 CSV format
    Returns: list of (dut_num, freq, v_mag, v_phase, i_mag, i_phase, pga_gain, tia_gain, valid)
    """
    with open(filepath, 'r') as f:
        lines = f.readlines()

    results = []
    dut_num = 1
    voltage_data = {}
    current_data = {}

    i = 0
    while i < len(lines):
        line = lines[i].strip()

        # Check for DUT header
        if line.startswith("========== DUT"):
            parts = line.split()
            if len(parts) >= 3:
                dut_num = int(parts[2])

        # Check for voltage section
        elif line.startswith("DUT_") and "VOLTAGE" in line:
            i += 1
            # Parse voltage data
            while i < len(lines):
                line = lines[i].strip()
                if line.startswith("DUT_") or line.startswith("="):
                    break

                parts = line.split(',')
                if len(parts) >= 6:
                    try:
                        freq = int(parts[0])
                        v_mag = int(parts[1])
                        v_phase = int(parts[2])
                        voltage_data[freq] = (v_mag, v_phase)
                    except ValueError:
                        pass
                i += 1
            continue

        # Check for current section
        elif line.startswith("DUT_") and "CURRENT" in line:
            i += 1
            # Parse current data
            while i < len(lines):
                line = lines[i].strip()
                if line.startswith("DUT_") or line.startswith("="):
                    break

                parts = line.split(',')
                if len(parts) >= 6:
                    try:
                        freq = int(parts[0])
                        i_mag = int(parts[1])
                        i_phase = int(parts[2])
                        pga_gain = int(parts[3])
                        tia_gain = int(parts[4])
                        valid = int(parts[5])
                        current_data[freq] = (i_mag, i_phase, pga_gain, tia_gain, valid)
                    except ValueError:
                        pass
                i += 1
            continue

        i += 1

    # Combine voltage and current data
    for freq in sorted(voltage_data.keys()):
        if freq in current_data:
            v_mag, v_phase = voltage_data[freq]
            i_mag, i_phase, pga_gain, tia_gain, valid = current_data[freq]
            results.append((dut_num, freq, v_mag, v_phase, i_mag, i_phase, pga_gain, tia_gain, valid))

    return results


def main():
    if len(sys.argv) != 2:
        print("Usage: python stm_to_impedance.py <input_csv>")
        print("\nExample: python stm_to_impedance.py DUT_PBS1x.csv")
        sys.exit(1)

    input_file = sys.argv[1]

    if not os.path.exists(input_file):
        print(f"ERROR: Input file not found: {input_file}")
        sys.exit(1)

    print(f"\n=== STM32 to Impedance Calculator ===")
    print(f"Input: {input_file}")

    # Parse STM32 data
    print("Parsing STM32 CSV data...")
    data = parse_stm_csv(input_file)
    print(f"  ✓ Parsed {len(data)} frequency points")

    # Calculate impedance
    print("Applying calibration and calculating impedance...")
    results = []

    for dut_num, freq, v_mag, v_phase, i_mag, i_phase, pga_gain, tia_gain, valid in data:
        if valid:
            Z_mag, Z_phase = calibrate_and_calculate(v_mag, v_phase, i_mag, i_phase,
                                                      freq, pga_gain, tia_gain)
            results.append((dut_num, freq, Z_mag, Z_phase))
            print(f"  Freq={freq:6d} Hz: |Z|={Z_mag:8.2f} Ω, Phase={Z_phase:7.2f}°")

    # Create Results directory if it doesn't exist
    os.makedirs("Results", exist_ok=True)

    # Save to CSV
    output_file = "Results/ESP.csv"
    print(f"\nSaving results to {output_file}...")

    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['DUT', 'Frequency_Hz', 'Magnitude_Ohms', 'Phase_Deg'])

        for dut_num, freq, Z_mag, Z_phase in results:
            writer.writerow([dut_num, freq, f"{Z_mag:.6f}", f"{Z_phase:.2f}"])

    print(f"  ✓ Saved {len(results)} results to {output_file}")
    print("\nDone!")


if __name__ == "__main__":
    main()
