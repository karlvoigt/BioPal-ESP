#!/usr/bin/env python3
"""
Test .pssession file generation
Creates a sample .pssession file and verifies it can be parsed
"""

import json
import math
from datetime import datetime

def create_test_pssession():
    """Create a test .pssession file with sample data"""

    # Sample impedance data (frequency sweep from 1 Hz to 100 kHz)
    test_data = []
    for i in range(10):
        freq = 10 ** (i / 3.0)  # Log-spaced frequencies
        mag = 10000 / (1 + freq/100)  # Decreasing impedance
        phase = -45  # -45 degrees

        test_data.append({
            'dut': 1,
            'frequency': freq,
            'magnitude': mag,
            'phase': phase
        })

    # Create method string
    method_str = f"""#PSTrace, Version=5.8.1704.29098
#BioPal ESP32 Impedance Analyzer
#METHOD_VERSION=1
#TECHNIQUE=14
#NOTES=BioPal ESP32 Test Measurement
#Frequency range
FREQ_START={test_data[0]['frequency']:.3E}
FREQ_END={test_data[-1]['frequency']:.3E}
NUM_FREQS={len(test_data)}
AMPLITUDE=1.000E-002
"""

    pssession = {
        "type": "PalmSens.DataFiles.SessionFile",
        "coreversion": "5.8.1704.29098",
        "methodformeasurement": method_str,
        "measurements": []
    }

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

    for point in test_data:
        freq = point['frequency']
        mag = point['magnitude']
        phase_deg = point['phase']
        phase_rad = math.radians(phase_deg)

        z_re = mag * math.cos(phase_rad)
        z_im = mag * math.sin(phase_rad)

        amplitude_v = 0.01
        i_ac = (amplitude_v / mag) * 1e6 if mag > 0 else 0

        freq_datavalues.append({"v": freq})
        z_re_datavalues.append({"v": z_re})
        z_im_datavalues.append({"v": z_im})
        z_mag_datavalues.append({"v": mag})
        z_phase_datavalues.append({"v": phase_deg})
        idc_datavalues.append({"v": 0.0, "c": 3, "s": 0})
        potential_datavalues.append({"v": 0.0, "s": 0, "r": 3})
        time_datavalues.append({"v": 0.0001})
        iac_datavalues.append({"v": i_ac, "c": 3, "s": 0})

    dataset = {
        "type": "PalmSens.Data.DataSetEIS",
        "values": [
            {
                "type": "PalmSens.Data.DataArrayCurrents",
                "arraytype": 2,
                "description": "Idc",
                "unit": {"type": "PalmSens.Units.MicroAmpere", "s": "A", "q": "Current", "a": "i"},
                "datavalues": idc_datavalues,
                "datavaluetype": "PalmSens.Data.DataValue.DataValueCurrentRange"
            },
            {
                "type": "PalmSens.Data.DataArrayPotentials",
                "arraytype": 1,
                "description": "potential",
                "unit": {"type": "PalmSens.Units.Volt", "s": "V", "q": "Potential", "a": "E"},
                "datavalues": potential_datavalues,
                "datavaluetype": "PalmSens.Data.DataValue.DataValueGainRange"
            },
            {
                "type": "PalmSens.Data.DataArrayTime",
                "arraytype": 0,
                "description": "time",
                "unit": {"type": "PalmSens.Units.Time", "s": "s", "q": "Time", "a": "t"},
                "datavalues": time_datavalues,
                "datavaluetype": "PalmSens.Data.DataValue.DataValue"
            },
            {
                "type": "PalmSens.Data.DataArray",
                "arraytype": 5,
                "description": "Frequency",
                "unit": {"type": "PalmSens.Units.Hertz", "s": "Hz", "q": "Frequency", "a": "f"},
                "datavalues": freq_datavalues,
                "datavaluetype": "PalmSens.Data.DataValue.DataValue"
            },
            {
                "type": "PalmSens.Data.DataArray",
                "arraytype": 7,
                "description": "ZRe",
                "unit": {"type": "PalmSens.Units.ZRe", "s": "Ω", "q": "Z'", "a": "Z"},
                "datavalues": z_re_datavalues,
                "datavaluetype": "PalmSens.Data.DataValue.DataValue"
            },
            {
                "type": "PalmSens.Data.DataArray",
                "arraytype": 8,
                "description": "ZIm",
                "unit": {"type": "PalmSens.Units.ZIm", "s": "Ω", "q": "-Z''", "a": "Z"},
                "datavalues": z_im_datavalues,
                "datavaluetype": "PalmSens.Data.DataValue.DataValue"
            },
            {
                "type": "PalmSens.Data.DataArray",
                "arraytype": 10,
                "description": "Z",
                "unit": {"type": "PalmSens.Units.Z", "s": "Ω", "q": "Z", "a": "Z"},
                "datavalues": z_mag_datavalues,
                "datavaluetype": "PalmSens.Data.DataValue.DataValue"
            },
            {
                "type": "PalmSens.Data.DataArray",
                "arraytype": 6,
                "description": "Phase",
                "unit": {"type": "PalmSens.Units.Phase", "s": "°", "q": "-Phase", "a": "Phase"},
                "datavalues": z_phase_datavalues,
                "datavaluetype": "PalmSens.Data.DataValue.DataValue"
            },
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
        "title": "Test DUT 1",
        "timestamp": int(datetime.now().timestamp() * 10000000),
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

    # Save file
    filepath = "test_output.pssession"
    json_str = json.dumps(pssession, indent=2)

    with open(filepath, 'w', encoding='utf-16-le') as f:
        f.write('\ufeff')
        f.write(json_str)
        f.write('\ufeff')

    print(f"✓ Created test file: {filepath}")
    print(f"  File size: {len(json_str.encode('utf-16-le'))} bytes")
    print(f"  Number of frequencies: {len(test_data)}")
    return filepath

def verify_pssession(filepath):
    """Verify a .pssession file can be loaded correctly"""
    print(f"\nVerifying: {filepath}")

    try:
        with open(filepath, 'r', encoding='utf-16-le') as f:
            content = f.read()

        # Strip BOM
        if content.startswith('\ufeff'):
            content = content[1:]

        # Parse just the first JSON object
        decoder = json.JSONDecoder()
        data, idx = decoder.raw_decode(content)

        print(f"✓ Successfully parsed JSON")
        print(f"  Type: {data.get('type')}")
        print(f"  Core version: {data.get('coreversion')}")
        print(f"  Number of measurements: {len(data.get('measurements', []))}")

        if data.get('measurements'):
            meas = data['measurements'][0]
            print(f"\n  First measurement:")
            print(f"    Title: {meas.get('title')}")
            print(f"    Device: {meas.get('deviceserial')}")

            dataset = meas.get('dataset', {})
            values = dataset.get('values', [])
            print(f"    Number of data arrays: {len(values)}")

            # Show frequency and impedance data
            for arr in values:
                desc = arr.get('description')
                if desc in ['Frequency', 'ZRe', 'ZIm', 'Z', 'Phase']:
                    datavals = arr.get('datavalues', [])
                    print(f"      {desc}: {len(datavals)} points")
                    if datavals:
                        print(f"        Range: {datavals[0]['v']:.3e} to {datavals[-1]['v']:.3e}")

        print("\n✓ File format is valid!")
        return True

    except Exception as e:
        print(f"✗ Error: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    print("=== PS Trace .pssession Format Test ===\n")

    # Create test file
    filepath = create_test_pssession()

    # Verify it
    verify_pssession(filepath)

    print("\n=== Test Complete ===")
