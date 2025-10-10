#!/usr/bin/env python3
"""
BioPal Calibration Tool
Generates calibration data by comparing PalmSens reference measurements with STM32 raw data.
Communicates directly with STM32 via UART (bypasses ESP32).
"""

import serial
import serial.tools.list_ports
import struct
import csv
import sys
import os
import time
import numpy as np
from datetime import datetime

# UART Constants
UART_BAUD_RATE = 115200  # Connect to USART2 (console/debug port)

# Command Protocol
CMD_START_BYTE = 0xAA
CMD_END_BYTE = 0x55
CMD_START_MEASUREMENT = 0x03

# Data packet types (from STM32)
PACKET_DUT_START = 0x10
PACKET_FREQUENCY = 0x11
PACKET_DUT_END = 0x12


class STM32Communication:
    """Handles UART communication with STM32"""

    def __init__(self, port, baud=UART_BAUD_RATE):
        self.port = port
        self.baud = baud
        self.ser = None

    def connect(self):
        """Open serial connection to STM32"""
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=10)
            time.sleep(2)  # Wait for connection to stabilize
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            print(f"✓ Connected to STM32 on {self.port} at {self.baud} baud")
            return True
        except Exception as e:
            print(f"✗ Failed to connect: {e}")
            return False

    def disconnect(self):
        """Close serial connection"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("✓ Disconnected from STM32")

    def send_start_command(self, num_duts=1):
        """Send START command to STM32"""
        # Clear buffer
        self.ser.reset_input_buffer()

        # Pack command: [START][CMD][data1:4][data2:4][data3:4][END]
        packet = struct.pack('<BB I I I B',
                            CMD_START_BYTE,
                            CMD_START_MEASUREMENT,
                            num_duts,  # data1 = number of DUTs
                            0,         # data2 = unused
                            0,         # data3 = unused
                            CMD_END_BYTE)

        self.ser.write(packet)
        time.sleep(0.1)
        print(f"→ Sent START command ({num_duts} DUT)")
        return True

    def receive_sweep_data(self):
        """Receive sweep data from STM32 via text CSV format"""
        print("\n=== Receiving Sweep Data ===")

        # Wait for DUT_1_VOLTAGE marker
        print("Waiting for DUT_1_VOLTAGE marker...")
        voltage_header = self._read_until_marker("DUT_1_VOLTAGE")
        if not voltage_header:
            print("✗ Timeout waiting for voltage data")
            return []

        # Parse voltage header to get frequency count
        parts = voltage_header.split(',')
        num_freqs = int(parts[1]) if len(parts) > 1 else 0
        print(f"← DUT 1 VOLTAGE ({num_freqs} frequencies)")

        # Read voltage data lines
        voltage_data = []
        for _ in range(num_freqs):
            line = self._read_line()
            if line:
                parts = line.split(',')
                if len(parts) >= 6:
                    freq_hz = int(parts[0])
                    magnitude = float(parts[1]) / 1000.0  # Convert from magnitude*1000
                    phase_deg = float(parts[2]) / 100.0   # Convert from phase*100
                    pga_gain = int(parts[3])
                    tia_gain = int(parts[4])
                    valid = bool(int(parts[5]))
                    voltage_data.append({
                        'freq': freq_hz,
                        'v_mag': magnitude,
                        'v_phase': phase_deg,
                        'pga_gain': pga_gain,
                        'tia_gain': tia_gain,
                        'valid': valid
                    })

        # Wait for DUT_1_CURRENT marker
        print("Waiting for DUT_1_CURRENT marker...")
        current_header = self._read_until_marker("DUT_1_CURRENT")
        if not current_header:
            print("✗ Timeout waiting for current data")
            return []

        parts = current_header.split(',')
        num_freqs = int(parts[1]) if len(parts) > 1 else 0
        print(f"← DUT 1 CURRENT ({num_freqs} frequencies)")

        # Read current data lines
        current_data = []
        for _ in range(num_freqs):
            line = self._read_line()
            if line:
                parts = line.split(',')
                if len(parts) >= 6:
                    freq_hz = int(parts[0])
                    magnitude = float(parts[1]) / 1000.0  # Convert from magnitude*1000
                    phase_deg = float(parts[2]) / 100.0   # Convert from phase*100
                    pga_gain = int(parts[3])
                    tia_gain = int(parts[4])
                    valid = bool(int(parts[5]))
                    current_data.append({
                        'freq': freq_hz,
                        'i_mag': magnitude,
                        'i_phase': phase_deg,
                        'pga_gain': pga_gain,
                        'tia_gain': tia_gain,
                        'valid': valid
                    })

        # Combine voltage and current data
        combined_data = []
        for v_point, i_point in zip(voltage_data, current_data):
            if v_point['freq'] == i_point['freq']:
                # Calculate phase difference (V - I)
                phase_diff = v_point['v_phase'] - i_point['i_phase']

                # Normalize phase to -180 to 180
                while phase_diff > 180:
                    phase_diff -= 360
                while phase_diff < -180:
                    phase_diff += 360

                combined_data.append({
                    'freq': v_point['freq'],
                    'v_mag': v_point['v_mag'],
                    'i_mag': i_point['i_mag'],
                    'phase': phase_diff,
                    'pga_gain': v_point['pga_gain'],
                    'tia_gain': v_point['tia_gain'],
                    'valid': v_point['valid'] and i_point['valid']
                })

                print(f"  {v_point['freq']} Hz: V={v_point['v_mag']:.3f}, I={i_point['i_mag']:.3f}, φ={phase_diff:.2f}°, PGA={v_point['pga_gain']}, TIA={v_point['tia_gain']}")

        print(f"✓ Received {len(combined_data)} frequency points\n")
        return combined_data

    def _read_until_marker(self, marker, timeout=120.0):
        """Read lines until marker is found"""
        start_time = time.time()
        while (time.time() - start_time) < timeout:
            if self.ser.in_waiting:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                print(f"  RX: {line}")  # ECHO EVERYTHING
                if marker in line:
                    return line
        return None

    def _read_line(self, timeout=5.0):
        """Read a single line"""
        start_time = time.time()
        while (time.time() - start_time) < timeout:
            if self.ser.in_waiting:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(f"  RX: {line}")  # ECHO EVERYTHING
                    return line
        return None


    def _old_receive_sweep_data(self):
        """OLD: Receive sweep data from STM32 via binary packets"""
        print("\n=== Receiving Sweep Data ===")

        data = []
        timeout_count = 0
        max_timeouts = 500  # 50 seconds total

        while timeout_count < max_timeouts:
            if self.ser.in_waiting > 0:
                # Read one byte and look for start marker
                byte = self.ser.read(1)
                if len(byte) == 0:
                    continue

                if byte[0] == CMD_START_BYTE:
                    # Read packet type
                    packet_type_byte = self.ser.read(1)
                    if len(packet_type_byte) == 0:
                        continue

                    packet_type = packet_type_byte[0]

                    if packet_type == PACKET_DUT_START:
                        # DUT START: [0xAA][0x10][dut][freq_count][res][res][0x55]
                        payload = self.ser.read(5)  # dut, freq_count, res, res, end
                        if len(payload) == 5:
                            dut, freq_count = payload[0], payload[1]
                            print(f"← DUT {dut} START ({freq_count} frequencies)")

                    elif packet_type == PACKET_FREQUENCY:
                        # FREQUENCY: [0xAA][0x11][freq:4][V_mag:4][V_phase:4][I_mag:4][I_phase:4][pga:1][tia:1][valid:1][0x55]
                        payload = self.ser.read(24)  # 23 data bytes + 1 end byte
                        if len(payload) == 24:
                            # Unpack frequency data
                            freq, v_mag, v_phase, i_mag, i_phase, pga_gain, tia_gain, valid = struct.unpack(
                                '<I I i I i BBB', payload[:23])

                            # Convert scaled values
                            v_mag_float = v_mag / 1000.0
                            i_mag_float = i_mag / 1000.0
                            v_phase_float = v_phase / 100.0
                            i_phase_float = i_phase / 100.0

                            # Phase difference (V - I)
                            phase_diff = v_phase_float - i_phase_float

                            # Normalize phase to -180 to 180
                            while phase_diff > 180:
                                phase_diff -= 360
                            while phase_diff < -180:
                                phase_diff += 360

                            point = {
                                'freq': freq,
                                'v_mag': v_mag_float,
                                'i_mag': i_mag_float,
                                'phase': phase_diff,
                                'pga_gain': pga_gain,
                                'tia_gain': tia_gain,
                                'valid': bool(valid)
                            }

                            data.append(point)
                            print(f"  {freq} Hz: V={v_mag_float:.3f}, I={i_mag_float:.3f}, φ={phase_diff:.2f}°, PGA={pga_gain}, TIA={tia_gain}")

                    elif packet_type == PACKET_DUT_END:
                        # DUT END: [0xAA][0x12][dut][0x55]
                        payload = self.ser.read(2)
                        if len(payload) == 2:
                            dut = payload[0]
                            print(f"← DUT {dut} END")
                            break  # Done receiving

                timeout_count = 0  # Reset timeout on successful read
            else:
                time.sleep(0.1)
                timeout_count += 1

        if timeout_count >= max_timeouts:
            print("✗ Timeout waiting for data")

        print(f"✓ Received {len(data)} frequency points\n")
        return data


class PalmSensParser:
    """Parse PalmSens PS Trace CSV files"""

    @staticmethod
    def parse_csv(filepath):
        """Parse PalmSens CSV export"""
        print(f"Loading PalmSens reference: {filepath}")

        # Try different encodings
        for encoding in ['utf-16-le', 'utf-16', 'utf-8', 'latin-1']:
            try:
                with open(filepath, 'r', encoding=encoding) as f:
                    lines = f.readlines()
                break
            except UnicodeDecodeError:
                continue
        else:
            raise ValueError("Could not decode PalmSens CSV file")

        # Find data start (line with "freq" and "Hz")
        data_start = 0
        for i, line in enumerate(lines):
            if 'freq' in line.lower() and 'hz' in line.lower():
                data_start = i + 1
                break

        if data_start == 0:
            raise ValueError("Could not find data header in PalmSens CSV")

        # Parse data rows
        data = []
        for line in lines[data_start:]:
            line = line.strip()
            if not line or line.startswith('�'):
                continue

            parts = [p.strip() for p in line.split(',')]
            if len(parts) < 4:
                continue

            try:
                # Columns: freq, neg_phase, Idc, Z_magnitude, ...
                freq = float(parts[0])
                neg_phase = float(parts[1])
                z_mag = float(parts[3])

                # Negate the phase (column is "neg. Phase")
                phase = -neg_phase

                # Normalize phase
                while phase > 180:
                    phase -= 360
                while phase < -180:
                    phase += 360

                data.append({
                    'freq': freq,
                    'z_mag': z_mag,
                    'phase': phase
                })
            except (ValueError, IndexError):
                continue

        if len(data) == 0:
            raise ValueError("No valid data in PalmSens CSV")

        print(f"✓ Loaded {len(data)} reference points")
        print(f"  Freq range: {data[0]['freq']:.1f} - {data[-1]['freq']:.1f} Hz")
        print(f"  Z range: {min(d['z_mag'] for d in data):.1f} - {max(d['z_mag'] for d in data):.1f} Ω\n")

        return data


class CalibrationCalculator:
    """Calculate calibration factors"""

    @staticmethod
    def average_sweeps(sweep1, sweep2):
        """Average two sweeps"""
        if len(sweep1) != len(sweep2):
            print(f"WARNING: Sweep lengths differ ({len(sweep1)} vs {len(sweep2)})")

        averaged = []

        for p1, p2 in zip(sweep1, sweep2):
            if p1['freq'] != p2['freq']:
                print(f"WARNING: Frequency mismatch ({p1['freq']} vs {p2['freq']})")
                continue

            # Average magnitudes
            v_mag_avg = (p1['v_mag'] + p2['v_mag']) / 2.0
            i_mag_avg = (p1['i_mag'] + p2['i_mag']) / 2.0

            # Circular mean for phase
            phase1_rad = np.radians(p1['phase'])
            phase2_rad = np.radians(p2['phase'])
            phase_avg_rad = np.arctan2(
                (np.sin(phase1_rad) + np.sin(phase2_rad)) / 2.0,
                (np.cos(phase1_rad) + np.cos(phase2_rad)) / 2.0
            )
            phase_avg = np.degrees(phase_avg_rad)

            averaged.append({
                'freq': p1['freq'],
                'v_mag': v_mag_avg,
                'i_mag': i_mag_avg,
                'phase': phase_avg,
                'pga_gain': p1['pga_gain'],
                'tia_gain': p1['tia_gain'],
                'valid': p1['valid'] and p2['valid']
            })

        return averaged

    @staticmethod
    def calculate_calibration(ps_data, stm_data):
        """Calculate calibration factors"""
        print("\n=== Calculating Calibration ===")

        # Create lookup dict for PalmSens data
        ps_lookup = {int(p['freq']): p for p in ps_data}

        cal_data = []

        for stm_point in stm_data:
            freq = int(stm_point['freq'])

            if freq not in ps_lookup:
                print(f"WARNING: No PalmSens reference for {freq} Hz - skipping")
                continue

            ps_point = ps_lookup[freq]

            # Calculate STM32 impedance
            if stm_point['i_mag'] <= 0:
                print(f"WARNING: Invalid current at {freq} Hz - skipping")
                continue

            z_stm = stm_point['v_mag'] / stm_point['i_mag']
            phase_stm = stm_point['phase']

            # Get PalmSens reference
            z_ps = ps_point['z_mag']
            phase_ps = ps_point['phase']

            # Calculate calibration factors
            z_mag_gain = z_ps / z_stm
            phase_offset = phase_ps - phase_stm

            # Normalize phase offset
            while phase_offset > 180:
                phase_offset -= 360
            while phase_offset < -180:
                phase_offset += 360

            cal_point = {
                'freq': freq,
                'tia_mode': 1 if stm_point['tia_gain'] else 0,  # 0=low TIA (tia_gain=False), 1=high TIA (tia_gain=True)
                'pga_gain': stm_point['pga_gain'],
                'z_mag_gain': z_mag_gain,
                'phase_offset': phase_offset,
                'z_stm': z_stm,
                'z_ps': z_ps,
                'error_pct': ((z_stm - z_ps) / z_ps) * 100
            }

            cal_data.append(cal_point)

            print(f"{freq:6d} Hz: Z_STM={z_stm:8.1f} Ω, Z_PS={z_ps:8.1f} Ω, gain={z_mag_gain:.6f}, Δφ={phase_offset:6.2f}°")

        print(f"\n✓ Calculated calibration for {len(cal_data)} points\n")
        return cal_data


class CalibrationFileManager:
    """Manage calibration CSV file"""

    @staticmethod
    def load_existing(filepath):
        """Load existing calibration data"""
        if not os.path.exists(filepath):
            return {}

        cal_dict = {}

        with open(filepath, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue

                parts = line.split(',')
                if len(parts) != 6:
                    continue

                try:
                    freq = int(parts[0])
                    tia_mode = int(parts[1])
                    pga_gain = int(parts[2])
                    z_mag_gain = float(parts[3])
                    unused = float(parts[4])
                    phase_offset = float(parts[5])

                    key = (freq, tia_mode, pga_gain)
                    cal_dict[key] = {
                        'z_mag_gain': z_mag_gain,
                        'unused': unused,
                        'phase_offset': phase_offset
                    }
                except ValueError:
                    continue

        return cal_dict

    @staticmethod
    def save(filepath, cal_data, existing_data=None):
        """Save calibration data (incremental update)"""
        print(f"\n=== Saving Calibration ===")

        # Merge with existing data
        if existing_data is None:
            existing_data = {}

        # Update with new calibration points
        for point in cal_data:
            key = (point['freq'], point['tia_mode'], point['pga_gain'])
            existing_data[key] = {
                'z_mag_gain': point['z_mag_gain'],
                'unused': 1.0,  # Not used (for backward compatibility)
                'phase_offset': point['phase_offset']
            }

        # Write to file
        with open(filepath, 'w') as f:
            f.write("# EIS Calibration Data\n")
            f.write("# Format: frequency_hz, tia_mode, pga_gain_index, z_mag_gain, unused, phase_offset_deg\n")
            f.write("# tia_mode: 0=low TIA, 1=high TIA\n")
            f.write("# pga_gain_index: 0-7 (maps to gains: 1, 2, 5, 10, 20, 50, 100, 200)\n")
            f.write("# z_mag_gain: Impedance magnitude calibration gain\n")
            f.write("# unused: Reserved (previously current_gain)\n\n")

            # Sort by frequency, tia_mode, pga_gain
            sorted_keys = sorted(existing_data.keys())

            for key in sorted_keys:
                freq, tia_mode, pga_gain = key
                cal = existing_data[key]
                f.write(f"{freq},{tia_mode},{pga_gain},{cal['z_mag_gain']:.6f},{cal['unused']:.6f},{cal['phase_offset']:.2f}\n")

        print(f"✓ Saved {len(existing_data)} calibration points to {filepath}\n")


def main():
    """Main calibration workflow"""
    print("\n" + "="*60)
    print("BioPal Calibration Tool")
    print("Generates calibration data using PalmSens reference")
    print("="*60 + "\n")

    # Parse arguments
    if len(sys.argv) != 3:
        print("Usage: python calibration_tool.py <palmsens_csv> <output_cal_csv>")
        print("\nExample:")
        print("  python calibration_tool.py dut1_pbs.csv calibration.csv")
        return

    ps_csv_path = sys.argv[1]
    output_csv_path = sys.argv[2]

    # Check input file exists
    if not os.path.exists(ps_csv_path):
        print(f"✗ PalmSens CSV not found: {ps_csv_path}")
        return

    # Load PalmSens reference data
    try:
        ps_data = PalmSensParser.parse_csv(ps_csv_path)
    except Exception as e:
        print(f"✗ Failed to parse PalmSens CSV: {e}")
        return

    # List serial ports
    print("=== Available Serial Ports ===")
    ports = serial.tools.list_ports.comports()
    for i, port in enumerate(ports):
        print(f"{i+1}. {port.device} - {port.description}")

    if len(ports) == 0:
        print("✗ No serial ports found")
        return

    # Select port
    try:
        choice = int(input("\nSelect STM32 port number: ")) - 1
        if choice < 0 or choice >= len(ports):
            print("✗ Invalid selection")
            return
        stm_port = ports[choice].device
    except (ValueError, KeyboardInterrupt):
        print("\n✗ Cancelled")
        return

    # Connect to STM32
    stm = STM32Communication(stm_port)
    if not stm.connect():
        return

    try:
        # Run 2 sweeps
        print("\n" + "="*60)
        print("SWEEP 1")
        print("="*60)
        if not stm.send_start_command(num_duts=1):
            print("✗ Failed to send START command")
            return
        sweep1 = stm.receive_sweep_data()

        time.sleep(1)

        print("\n" + "="*60)
        print("SWEEP 2")
        print("="*60)
        if not stm.send_start_command(num_duts=1):
            print("✗ Failed to send START command")
            return
        sweep2 = stm.receive_sweep_data()

        # Average sweeps
        print("=== Averaging Sweeps ===")
        averaged_data = CalibrationCalculator.average_sweeps(sweep1, sweep2)
        print(f"✓ Averaged {len(averaged_data)} points\n")

        # Calculate calibration
        cal_data = CalibrationCalculator.calculate_calibration(ps_data, averaged_data)

        # Load existing calibration
        existing_cal = CalibrationFileManager.load_existing(output_csv_path)
        if len(existing_cal) > 0:
            print(f"→ Loaded {len(existing_cal)} existing calibration points")

        # Save calibration (incremental)
        CalibrationFileManager.save(output_csv_path, cal_data, existing_cal)

        print("="*60)
        print("✓ Calibration Complete!")
        print("="*60)

    finally:
        stm.disconnect()


if __name__ == "__main__":
    main()
