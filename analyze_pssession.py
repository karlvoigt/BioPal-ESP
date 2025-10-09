#!/usr/bin/env python3
"""
Analyze PS Trace .pssession file structure
"""

import json
import sys

def analyze_value(val, depth=0, max_depth=5):
    """Recursively analyze a value and return its structure"""
    indent = "  " * depth

    if depth > max_depth:
        return f"{indent}[max depth reached]"

    if isinstance(val, dict):
        result = f"{indent}{{\n"
        for i, (key, value) in enumerate(list(val.items())[:10]):  # Show first 10 keys
            result += f"{indent}  '{key}': {analyze_value(value, depth+1, max_depth)}\n"
        if len(val) > 10:
            result += f"{indent}  ... ({len(val) - 10} more keys)\n"
        result += f"{indent}}}"
        return result
    elif isinstance(val, list):
        if len(val) == 0:
            return "[]"
        result = f"[\n"
        # Show first few items
        num_to_show = min(3, len(val))
        for i in range(num_to_show):
            result += f"{indent}  [{i}]: {analyze_value(val[i], depth+1, max_depth)}\n"
        if len(val) > num_to_show:
            result += f"{indent}  ... ({len(val) - num_to_show} more items)\n"
        result += f"{indent}]"
        return result
    elif isinstance(val, str):
        if len(val) > 50:
            return f'"{val[:50]}..." (len={len(val)})'
        return f'"{val}"'
    elif isinstance(val, (int, float)):
        return str(val)
    elif isinstance(val, bool):
        return str(val)
    elif val is None:
        return "null"
    else:
        return f"<{type(val).__name__}>"

def analyze_pssession(filepath):
    """Analyze .pssession file structure"""
    print(f"Analyzing: {filepath}\n")

    # First, read binary to check for BOM
    with open(filepath, 'rb') as f:
        raw_bytes = f.read()

    print(f"File size: {len(raw_bytes)} bytes")
    print(f"First 20 bytes: {raw_bytes[:20].hex()}")
    print(f"First 20 bytes (repr): {repr(raw_bytes[:20])}")
    print()

    # Try reading with different encodings
    encodings = ['utf-16-le', 'utf-16', 'utf-8', 'utf-8-sig']

    for encoding in encodings:
        try:
            print(f"Trying encoding: {encoding}")
            with open(filepath, 'r', encoding=encoding) as f:
                content = f.read()

            # Strip BOM if present
            if content.startswith('\ufeff'):
                print("  Stripping BOM...")
                content = content[1:]

            # Show first 500 characters
            print(f"\nFirst 500 characters:")
            print(repr(content[:500]))
            print()

            # Try to parse as JSON - might be multiple objects
            print(f"Attempting to parse JSON...")

            # First try as single object
            try:
                data = json.loads(content)
            except json.JSONDecodeError as e:
                if "Extra data" in str(e):
                    # Try to parse multiple JSON objects
                    print(f"  Found extra data at position {e.pos}")
                    print(f"  Trying to parse first JSON object only...")

                    # Use JSONDecoder to get just the first object
                    decoder = json.JSONDecoder()
                    data, idx = decoder.raw_decode(content)

                    print(f"  ✓ Successfully parsed first JSON object (ends at position {idx})")
                    print(f"  Content after JSON object ({len(content) - idx} chars):")
                    print(repr(content[idx:idx+200]))
                else:
                    raise

            print(f"✓ Successfully loaded with encoding: {encoding}\n")
            print("="*80)
            print("JSON STRUCTURE")
            print("="*80)

            # Print top-level keys and types
            print(f"\nTop-level type: {type(data).__name__}")

            if isinstance(data, dict):
                print(f"Number of top-level keys: {len(data)}")
                print("\nTop-level keys:")
                for key in data.keys():
                    val_type = type(data[key]).__name__
                    if isinstance(data[key], list):
                        val_type += f" (length={len(data[key])})"
                    elif isinstance(data[key], dict):
                        val_type += f" (keys={len(data[key])})"
                    print(f"  - {key}: {val_type}")

            print("\n" + "="*80)
            print("DETAILED STRUCTURE")
            print("="*80)
            print(analyze_value(data, depth=0, max_depth=4))

            print("\n" + "="*80)
            print("SPECIFIC DATA ANALYSIS")
            print("="*80)

            # Look for measurements
            if 'measurements' in data:
                measurements = data['measurements']
                print(f"\nNumber of measurements: {len(measurements)}")

                if len(measurements) > 0:
                    print(f"\nFirst measurement structure:")
                    first_meas = measurements[0]

                    if isinstance(first_meas, dict):
                        for key, val in first_meas.items():
                            if key == 'dataset' and isinstance(val, list):
                                print(f"  {key}: list with {len(val)} arrays")
                                for i, arr in enumerate(val[:5]):  # Show first 5
                                    if isinstance(arr, dict):
                                        arr_type = arr.get('type', 'unknown')
                                        x_label = arr.get('xaxislabel', 'unknown')
                                        y_label = arr.get('yaxislabel', 'unknown')
                                        num_points = len(arr.get('xvalues', []))
                                        print(f"    [{i}] {y_label} vs {x_label} ({num_points} points) - type: {arr_type}")
                            else:
                                val_str = str(val)
                                if len(val_str) > 100:
                                    val_str = val_str[:100] + "..."
                                print(f"  {key}: {val_str}")

            # Look for method
            if 'methodformeasurement' in data:
                print(f"\n\nMethod for measurement:")
                method = data['methodformeasurement']
                if isinstance(method, dict):
                    for key, val in list(method.items())[:20]:  # Show first 20 keys
                        val_str = str(val)
                        if len(val_str) > 100:
                            val_str = val_str[:100] + "..."
                        print(f"  {key}: {val_str}")

            # Sample data values - explore dataset structure
            print(f"\n\nDataset structure:")
            if 'measurements' in data and len(data['measurements']) > 0:
                first_meas = data['measurements'][0]
                if 'dataset' in first_meas:
                    dataset = first_meas['dataset']
                    print(f"  Dataset type: {dataset.get('type')}")

                    if 'values' in dataset and isinstance(dataset['values'], list):
                        print(f"  Number of value arrays: {len(dataset['values'])}")

                        # Show structure of each array
                        for i, arr in enumerate(dataset['values']):
                            if isinstance(arr, dict):
                                arr_type = arr.get('type', 'unknown')
                                arraytype = arr.get('arraytype', 'unknown')
                                description = arr.get('description', 'unknown')
                                unit = arr.get('unit', '')

                                print(f"\n  Array [{i}]: {description} ({unit})")
                                print(f"    type: {arr_type}")
                                print(f"    arraytype: {arraytype}")

                                # Show datavalues structure
                                if 'datavalues' in arr and isinstance(arr['datavalues'], list):
                                    datavals = arr['datavalues']
                                    print(f"    datavalues: {len(datavals)} items")
                                    if len(datavals) > 0:
                                        print(f"      First 3 items: {datavals[:3]}")
                                        print(f"      Type of first item: {type(datavals[0])}")

            return data

        except json.JSONDecodeError as e:
            print(f"✗ JSON decode error with {encoding}: {e}\n")
            continue
        except UnicodeDecodeError as e:
            print(f"✗ Unicode error with {encoding}: {e}\n")
            continue
        except Exception as e:
            print(f"✗ Error with {encoding}: {e}\n")
            continue

    print("\nFailed to load with any encoding")
    return None

if __name__ == "__main__":
    filepath = "PBS 1x DUT 1_2.pssession"

    if len(sys.argv) > 1:
        filepath = sys.argv[1]

    analyze_pssession(filepath)
