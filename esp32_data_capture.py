#!/usr/bin/env python3
"""
ESP32 BioPal Data Capture Tool
Captures impedance data from ESP32 and exports to CSV and PS Trace .pssession format
OR converts existing CSV file to .pssession format
"""

import serial
import serial.tools.list_ports
import json
import sys
import os
from datetime import datetime
import math
import argparse

class ESP32DataCapture:
    def __init__(self):
        self.ser = None
        self.capturing_csv = False
        self.csv_lines = []

    def list_serial_ports(self):
        """List all available serial ports"""
        ports = serial.tools.list_ports.comports()
        available_ports = []

        print("\n=== Available Serial Ports ===")
        for i, port in enumerate(ports):
            print(f"{i+1}. {port.device} - {port.description}")
            available_ports.append(port.device)

        return available_ports

    def connect(self, port, baud_rate=115200):
        """Connect to ESP32"""
        try:
            self.ser = serial.Serial(port, baud_rate, timeout=1)
            print(f"\nConnected to {port} at {baud_rate} baud")
            return True
        except Exception as e:
            print(f"ERROR: Failed to connect to {port}: {e}")
            return False

    def send_command(self, command):
        """Send command to ESP32"""
        if self.ser and self.ser.is_open:
            self.ser.write(f"{command}\n".encode())
            print(f">>> Sent: {command}")
        else:
            print("ERROR: Serial port not open")

    def process_line(self, line):
        """Process incoming line from ESP32"""
        # Check for CSV data markers
        if "========== IMPEDANCE DATA CSV ==========" in line:
            self.capturing_csv = True
            self.csv_lines = []
            print(line)  # Mirror to console
            return

        if self.capturing_csv:
            if "========================================" in line and len(self.csv_lines) > 0:
                # End of CSV data
                print(line)  # Mirror to console
                self.capturing_csv = False
                self.save_data()
                return
            elif line.strip() and not line.startswith("==="):
                # CSV data line
                self.csv_lines.append(line.strip())

        # Mirror all output to console
        print(line, end='')

    def parse_csv_data(self):
        """Parse CSV lines into structured data"""
        data = []

        for line in self.csv_lines[1:]:  # Skip header line
            parts = line.split(',')
            if len(parts) >= 4:
                try:
                    dut = int(parts[0])
                    freq = float(parts[1])
                    magnitude = float(parts[2])
                    phase = float(parts[3])

                    data.append({
                        'dut': dut,
                        'frequency': freq,
                        'magnitude': magnitude,
                        'phase': phase
                    })
                except ValueError:
                    continue

        return data

    def save_data(self, save_csv=True):
        """Prompt for filename and save CSV and/or .pssession files"""
        if not self.csv_lines:
            print("ERROR: No CSV data to save")
            return

        # Prompt for filename
        print("\n=== Save Data ===")
        filename = input("Enter filename (without extension): ").strip()

        if not filename:
            print("ERROR: No filename provided, data not saved")
            return

        # Save raw CSV if requested
        if save_csv:
            csv_path = f"{filename}.csv"
            try:
                with open(csv_path, 'w') as f:
                    for line in self.csv_lines:
                        f.write(line + '\n')
                print(f"✓ Saved CSV to: {csv_path}")
            except Exception as e:
                print(f"ERROR: Failed to save CSV: {e}")
                return

        # Parse data and create .pssession file
        data = self.parse_csv_data()
        if data:
            pssession_path = f"{filename}.pssession"
            self.create_pssession_file(data, pssession_path)
        else:
            print("WARNING: Could not parse CSV data for .pssession export")

    def create_pssession_file(self, data, filepath):
        """Create PS Trace .pssession file from impedance data"""
        # Group data by DUT
        dut_groups = {}
        for point in data:
            dut = point['dut']
            if dut not in dut_groups:
                dut_groups[dut] = []
            dut_groups[dut].append(point)

        # Get frequency range from data
        all_freqs = [p['frequency'] for p in data]
        freq_min = min(all_freqs)
        freq_max = max(all_freqs)
        num_freqs = len(set(all_freqs))

        # Create method string (simplified version)
        method_str = f"""#PSTrace, Version=5.8.1704.29098
#BioPal ESP32 Impedance Analyzer
#METHOD_VERSION=1
#TECHNIQUE=14
#NOTES=BioPal ESP32 Measurement
#Frequency range
FREQ_START={freq_min:.3E}
FREQ_END={freq_max:.3E}
NUM_FREQS={num_freqs}
AMPLITUDE=1.000E-002
"""

        # Create PS Trace compatible structure matching the actual format
        pssession = {
            "type": "PalmSens.DataFiles.SessionFile",
            "coreversion": "5.8.1704.29098",
            "methodformeasurement": method_str,
            "measurements": []
        }

        # Create measurement for each DUT
        for dut_num in sorted(dut_groups.keys()):
            dut_data = dut_groups[dut_num]

            # Build data arrays
            freq_datavalues = []
            z_re_datavalues = []
            z_im_datavalues = []
            z_mag_datavalues = []
            z_phase_datavalues = []
            idc_datavalues = []
            potential_datavalues = []
            time_datavalues = []
            iac_datavalues = []

            for point in dut_data:
                freq = point['frequency']
                mag = point['magnitude']
                phase_deg = point['phase']
                phase_rad = math.radians(phase_deg)

                # Calculate ZRe and ZIm from magnitude and phase
                # Z = |Z| * e^(j*phase)
                # Re(Z) = |Z| * cos(phase)
                # Im(Z) = |Z| * sin(phase) (note: positive for phase)
                z_re = mag * math.cos(phase_rad)
                z_im = mag * math.sin(phase_rad)

                # Calculate Iac from Z magnitude (assuming 10mV AC amplitude)
                amplitude_v = 0.01  # 10mV
                i_ac = (amplitude_v / mag) * 1e6 if mag > 0 else 0  # Convert to µA

                freq_datavalues.append({"v": freq})
                z_re_datavalues.append({"v": z_re})
                z_im_datavalues.append({"v": z_im})
                z_mag_datavalues.append({"v": mag})
                z_phase_datavalues.append({"v": phase_deg})

                # Mock current/potential/time data (required by PS Trace)
                idc_datavalues.append({"v": 0.0, "c": 3, "s": 0})
                potential_datavalues.append({"v": 0.0, "s": 0, "r": 3})
                time_datavalues.append({"v": 0.0001})
                iac_datavalues.append({"v": i_ac, "c": 3, "s": 0})

            # Build dataset with proper PalmSens structure
            dataset = {
                "type": "PalmSens.Data.DataSetEIS",
                "values": [
                    # Array 0: Idc
                    {
                        "type": "PalmSens.Data.DataArrayCurrents",
                        "arraytype": 2,
                        "description": "Idc",
                        "unit": {"type": "PalmSens.Units.MicroAmpere", "s": "A", "q": "Current", "a": "i"},
                        "datavalues": idc_datavalues,
                        "datavaluetype": "PalmSens.Data.DataValue.DataValueCurrentRange"
                    },
                    # Array 1: potential
                    {
                        "type": "PalmSens.Data.DataArrayPotentials",
                        "arraytype": 1,
                        "description": "potential",
                        "unit": {"type": "PalmSens.Units.Volt", "s": "V", "q": "Potential", "a": "E"},
                        "datavalues": potential_datavalues,
                        "datavaluetype": "PalmSens.Data.DataValue.DataValueGainRange"
                    },
                    # Array 2: time
                    {
                        "type": "PalmSens.Data.DataArrayTime",
                        "arraytype": 0,
                        "description": "time",
                        "unit": {"type": "PalmSens.Units.Time", "s": "s", "q": "Time", "a": "t"},
                        "datavalues": time_datavalues,
                        "datavaluetype": "PalmSens.Data.DataValue.DataValue"
                    },
                    # Array 3: Frequency
                    {
                        "type": "PalmSens.Data.DataArray",
                        "arraytype": 5,
                        "description": "Frequency",
                        "unit": {"type": "PalmSens.Units.Hertz", "s": "Hz", "q": "Frequency", "a": "f"},
                        "datavalues": freq_datavalues,
                        "datavaluetype": "PalmSens.Data.DataValue.DataValue"
                    },
                    # Array 4: ZRe
                    {
                        "type": "PalmSens.Data.DataArray",
                        "arraytype": 7,
                        "description": "ZRe",
                        "unit": {"type": "PalmSens.Units.ZRe", "s": "Ω", "q": "Z'", "a": "Z"},
                        "datavalues": z_re_datavalues,
                        "datavaluetype": "PalmSens.Data.DataValue.DataValue"
                    },
                    # Array 5: ZIm
                    {
                        "type": "PalmSens.Data.DataArray",
                        "arraytype": 8,
                        "description": "ZIm",
                        "unit": {"type": "PalmSens.Units.ZIm", "s": "Ω", "q": "-Z''", "a": "Z"},
                        "datavalues": z_im_datavalues,
                        "datavaluetype": "PalmSens.Data.DataValue.DataValue"
                    },
                    # Array 6: Z (magnitude)
                    {
                        "type": "PalmSens.Data.DataArray",
                        "arraytype": 10,
                        "description": "Z",
                        "unit": {"type": "PalmSens.Units.Z", "s": "Ω", "q": "Z", "a": "Z"},
                        "datavalues": z_mag_datavalues,
                        "datavaluetype": "PalmSens.Data.DataValue.DataValue"
                    },
                    # Array 7: Phase
                    {
                        "type": "PalmSens.Data.DataArray",
                        "arraytype": 6,
                        "description": "Phase",
                        "unit": {"type": "PalmSens.Units.Phase", "s": "°", "q": "-Phase", "a": "Phase"},
                        "datavalues": z_phase_datavalues,
                        "datavaluetype": "PalmSens.Data.DataValue.DataValue"
                    },
                    # Array 8: Iac
                    {
                        "type": "PalmSens.Data.DataArrayCurrents",
                        "arraytype": 9,
                        "description": "Iac",
                        "unit": {"type": "PalmSens.Units.MicroAmpere", "s": "A", "q": "Current", "a": "i"},
                        "datavalues": iac_datavalues,
                        "datavaluetype": "PalmSens.Data.DataValue.DataValueCurrentRange"
                    }
                ]
            }

            measurement = {
                "title": f"DUT {dut_num}",
                "timestamp": int(datetime.now().timestamp() * 10000000),  # .NET ticks approximation
                "utctimestamp": int(datetime.now().timestamp() * 10000000),
                "deviceused": 0,
                "deviceserial": "BioPal-ESP32",
                "devicefw": "1.0",
                "type": ".",
                "dataset": dataset,
                "method": method_str,
                "curves": [],
                "eisdatalist": []
            }

            pssession["measurements"].append(measurement)

        # Save as UTF-16 LE encoded JSON with BOM
        try:
            json_str = json.dumps(pssession, indent=2)
            # Add BOM at start, write JSON, add BOM at end (matching original format)
            with open(filepath, 'w', encoding='utf-16-le') as f:
                f.write('\ufeff')  # BOM
                f.write(json_str)
                f.write('\ufeff')  # BOM at end
            print(f"✓ Saved .pssession to: {filepath}")
        except Exception as e:
            print(f"ERROR: Failed to save .pssession: {e}")

    def run(self):
        """Main program loop"""
        print("\n=== ESP32 BioPal Data Capture ===")

        # List and select serial port
        ports = self.list_serial_ports()

        if not ports:
            print("ERROR: No serial ports found")
            return

        try:
            choice = int(input("\nSelect port number: ")) - 1
            if choice < 0 or choice >= len(ports):
                print("ERROR: Invalid port selection")
                return
        except ValueError:
            print("ERROR: Invalid input")
            return

        # Connect to selected port
        if not self.connect(ports[choice]):
            return

        print("\n=== Commands ===")
        print("start [num_duts]  - Start measurement (e.g., 'start 4')")
        print("stop              - Stop measurement")
        print("help              - Show ESP32 help")
        print("quit              - Exit program")
        print("\nType commands below (output from ESP32 will be mirrored):\n")

        # Use threading for cross-platform input handling
        import threading
        import queue

        input_queue = queue.Queue()
        running = True

        def input_thread():
            """Thread to handle user input"""
            while running:
                try:
                    line = input()
                    input_queue.put(line)
                except EOFError:
                    break

        # Start input thread
        thread = threading.Thread(target=input_thread, daemon=True)
        thread.start()

        # Main loop
        try:
            while True:
                # Check for user input from queue
                try:
                    line = input_queue.get_nowait()

                    if line.lower() == 'quit':
                        print("\nExiting...")
                        break

                    if line:
                        self.send_command(line)
                except queue.Empty:
                    pass

                # Read from serial port
                if self.ser.in_waiting > 0:
                    try:
                        line = self.ser.readline().decode('utf-8', errors='ignore')
                        self.process_line(line)
                    except Exception as e:
                        print(f"ERROR reading serial: {e}")

                # Small delay to prevent CPU spinning
                import time
                time.sleep(0.01)

        except KeyboardInterrupt:
            print("\n\nInterrupted by user")

        finally:
            running = False
            if self.ser and self.ser.is_open:
                self.ser.close()
                print("Serial port closed")


def convert_csv_to_pssession(csv_filepath):
    """Convert existing CSV file to .pssession format"""
    print(f"\n=== Converting CSV to .pssession ===")
    print(f"Input file: {csv_filepath}")

    # Check if file exists
    if not os.path.exists(csv_filepath):
        print(f"ERROR: File not found: {csv_filepath}")
        return

    # Read CSV file
    try:
        with open(csv_filepath, 'r') as f:
            lines = f.readlines()
    except Exception as e:
        print(f"ERROR: Failed to read CSV file: {e}")
        return

    # Remove any whitespace and filter empty lines
    csv_lines = [line.strip() for line in lines if line.strip()]

    if not csv_lines or len(csv_lines) < 2:
        print("ERROR: CSV file is empty or has no data")
        return

    # Validate header
    header = csv_lines[0]
    if not header.startswith("DUT,Frequency"):
        print(f"WARNING: Unexpected CSV header: {header}")
        print("Expected format: DUT,Frequency_Hz,Magnitude_Ohms,Phase_Deg")

    # Parse CSV data
    data = []
    for line in csv_lines[1:]:  # Skip header
        parts = line.split(',')
        if len(parts) >= 4:
            try:
                dut = int(parts[0])
                freq = float(parts[1])
                magnitude = float(parts[2])
                phase = float(parts[3])

                data.append({
                    'dut': dut,
                    'frequency': freq,
                    'magnitude': magnitude,
                    'phase': phase
                })
            except ValueError:
                print(f"WARNING: Skipping invalid line: {line}")
                continue

    if not data:
        print("ERROR: No valid data found in CSV file")
        return

    print(f"✓ Parsed {len(data)} data points from CSV")

    # Determine output filename
    base_name = os.path.splitext(csv_filepath)[0]
    output_path = f"{base_name}.pssession"

    # Check if output file already exists
    if os.path.exists(output_path):
        response = input(f"File {output_path} already exists. Overwrite? (y/n): ").strip().lower()
        if response != 'y':
            print("Conversion cancelled")
            return

    # Create .pssession file
    capture = ESP32DataCapture()
    capture.create_pssession_file(data, output_path)

    print(f"\n✓ Conversion complete!")
    print(f"  CSV: {csv_filepath}")
    print(f"  .pssession: {output_path}")


def main():
    """Main entry point"""
    # Parse command-line arguments
    parser = argparse.ArgumentParser(
        description='ESP32 BioPal Data Capture Tool',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Live capture from ESP32
  python esp32_data_capture.py

  # Convert existing CSV to .pssession
  python esp32_data_capture.py --convert data.csv
  python esp32_data_capture.py -c data.csv
        '''
    )
    parser.add_argument('-c', '--convert', metavar='CSV_FILE',
                        help='Convert existing CSV file to .pssession format')

    args = parser.parse_args()

    try:
        if args.convert:
            # Convert mode
            convert_csv_to_pssession(args.convert)
        else:
            # Live capture mode
            capture = ESP32DataCapture()
            capture.run()
    except Exception as e:
        print(f"FATAL ERROR: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    main()
