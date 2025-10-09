# ESP32 BioPal Data Capture Tool

Python tool for capturing impedance data from ESP32 and exporting to CSV and PS Trace formats.

## Installation

Install dependencies:
```bash
pip install -r requirements.txt
```

## Usage

### Mode 1: Live Capture from ESP32

1. **Connect your ESP32 via USB**

2. **Run the data capture script:**
   ```bash
   python esp32_data_capture.py
   ```

3. **Select your ESP32** from the list of serial ports

4. **Send commands to the ESP32:**
   - `start 4` - Start measurement with 4 DUTs
   - `start 1` - Start measurement with 1 DUT
   - `stop` - Stop measurement
   - `help` - Show ESP32 help
   - `quit` - Exit program

5. **Monitor progress:**
   - All output from ESP32 will be mirrored to your console
   - You'll see real-time updates as measurements progress
   - Bode plots are drawn on the TFT display after each DUT completes

6. **Save data:**
   - When measurements complete, CSV data will be printed to console
   - You'll be prompted to enter a filename
   - Two files will be automatically saved:
     - `{filename}.csv` - Raw CSV data
     - `{filename}.pssession` - PS Trace compatible file

### Mode 2: Convert Existing CSV to .pssession

If you already have a CSV file and just want to convert it to PS Trace format:

```bash
python esp32_data_capture.py --convert yourfile.csv
# or
python esp32_data_capture.py -c yourfile.csv
```

This will:
- Read the existing CSV file
- Parse the impedance data
- Create a `.pssession` file with the same base name
- Not create a new CSV file (since you already have one)

## File Formats

### CSV Format
```
DUT,Frequency_Hz,Magnitude_Ohms,Phase_Deg
1,100,12345.67,45.23
1,200,11234.56,43.12
...
```

### .pssession Format
- UTF-16 LE encoded JSON file
- Can be opened directly in PalmSens PS Trace software
- Contains full EIS dataset with:
  - Frequency sweep data
  - Impedance (Z, ZRe, ZIm)
  - Phase angle
  - Current and voltage data
  - Method parameters

## Testing

Test the .pssession file generator:
```bash
python test_pssession_format.py
```

This creates a test file `test_output.pssession` with sample data.

## Analyzing .pssession Files

To analyze the structure of a .pssession file:
```bash
python analyze_pssession.py <filename.pssession>
```

This will show:
- File encoding and structure
- All data arrays and their contents
- Method parameters
- Sample data values

## Troubleshooting

- **Port not found**: Make sure ESP32 is connected and drivers are installed
- **Permission denied**: On Linux/Mac, you may need `sudo` or add user to dialout group
- **Connection timeout**: Check baud rate (should be 115200)
- **CSV not captured**: Make sure ESP32 firmware sends CSV data with proper delimiters

## Notes

- Use Ctrl+C to force exit if needed
- The script uses threading for cross-platform compatibility
- .pssession files include BOM markers at start and end (required by PS Trace)
- All data is captured in real-time as ESP32 sends it
