#!/usr/bin/env python3
"""
Impedance Comparison GUI
Compares PalmSens PS Trace data with BioPal ESP32 data
Shows Bode plots and percentage error analysis
"""

import matplotlib.pyplot as plt
import numpy as np
import csv
import sys
import os
import tkinter as tk
from tkinter import filedialog

class ImpedanceComparison:
    def __init__(self):
        self.ps_data = None
        self.bp_data = None

    def parse_palmsens_csv(self, filepath):
        """Parse PalmSens PS Trace CSV format (supports multiple runs for averaging)"""
        print(f"Loading PalmSens CSV: {filepath}")

        # Try different encodings (UTF-16 first as PalmSens often uses it)
        # Note: latin-1 must be last as it accepts almost anything but may mangle text
        for encoding in ['utf-16', 'utf-16-le', 'utf-8-sig', 'utf-8', 'latin-1']:
            try:
                with open(filepath, 'r', encoding=encoding) as f:
                    lines = f.readlines()
                # Verify we got actual text (not mangled)
                if lines and any('freq' in line.lower() for line in lines):
                    print(f"  Successfully decoded with {encoding} encoding")
                    break
            except (UnicodeDecodeError, UnicodeError):
                continue
        else:
            raise ValueError("Could not decode CSV file with any known encoding")

        # Find all data sections (multiple runs)
        all_runs = []
        i = 0

        while i < len(lines):
            line_lower = lines[i].lower()
            # Look for header with freq/hz AND (phase OR z/impedance)
            if ('freq' in line_lower and 'hz' in line_lower and
                ('phase' in line_lower or 'z' in line_lower or 'ohm' in line_lower)):

                print(f"  Found data section header at line {i+1}")

                # Found a data section header, parse this run
                run_freq = []
                run_z_mag = []
                run_phase = []
                i += 1  # Move to first data line

                while i < len(lines):
                    line = lines[i].strip()

                    # Check if we hit another header (new run) or end of data
                    line_lower = line.lower()
                    if ('freq' in line_lower and 'hz' in line_lower and
                        ('phase' in line_lower or 'z' in line_lower or 'ohm' in line_lower)):
                        # Found next run header, don't increment i
                        break

                    # Skip empty lines, BOM characters, or lines with only commas
                    if not line or line.startswith('\ufeff') or line.startswith('�') or line.replace(',', '').strip() == '':
                        i += 1
                        continue

                    parts = [p.strip() for p in line.split(',')]
                    if len(parts) < 4:
                        i += 1
                        continue

                    try:
                        # Column 0: Frequency
                        # Column 1: Phase (negative)
                        # Column 3: Z magnitude
                        f = float(parts[0])
                        p = float(parts[1])
                        z = float(parts[3])

                        # Skip if frequency or impedance is 0 or negative
                        if f <= 0 or z <= 0:
                            i += 1
                            continue

                        run_freq.append(f)
                        run_phase.append(-p)  # Negate the phase
                        run_z_mag.append(z)
                    except (ValueError, IndexError):
                        pass

                    i += 1

                if run_freq:
                    all_runs.append({
                        'freq': run_freq,
                        'z_mag': run_z_mag,
                        'phase': run_phase
                    })
            else:
                i += 1

        if len(all_runs) == 0:
            print("  ERROR: Could not find data header in PalmSens CSV")
            print(f"  First 10 lines of file:")
            for i, line in enumerate(lines[:10]):
                print(f"    Line {i+1}: {line.strip()[:80]}")
            raise ValueError("Could not find data header in PalmSens CSV")

        num_runs = len(all_runs)
        print(f"  Detected {num_runs} measurement run(s)")

        # If multiple runs, average them
        if num_runs > 1:
            print(f"  Averaging {num_runs} runs...")
            averaged_data = self._average_palmsens_runs(all_runs)
            print(f"  ✓ Loaded and averaged {len(averaged_data['freq'])} data points")
        else:
            freq = np.array(all_runs[0]['freq'])
            z_mag = np.array(all_runs[0]['z_mag'])
            phase = np.array(all_runs[0]['phase'])

            # Normalize phase to -180 to +180
            phase = np.arctan2(np.sin(np.radians(phase)), np.cos(np.radians(phase)))
            phase = np.degrees(phase)

            averaged_data = {'freq': freq, 'z_mag': z_mag, 'phase': phase}
            print(f"  ✓ Loaded {len(freq)} data points")

        # Print data range for debugging
        print(f"  Frequency range: {averaged_data['freq'].min():.1f} Hz to {averaged_data['freq'].max():.1f} Hz")
        print(f"  Magnitude range: {averaged_data['z_mag'].min():.1f} Ω to {averaged_data['z_mag'].max():.1f} Ω")
        print(f"  Phase range: {averaged_data['phase'].min():.2f}° to {averaged_data['phase'].max():.2f}°")

        return averaged_data

    def _average_palmsens_runs(self, runs):
        """Average multiple PalmSens measurement runs using circular mean for phase"""
        # Group measurements by frequency
        freq_to_measurements = {}

        for run in runs:
            for f, z, p in zip(run['freq'], run['z_mag'], run['phase']):
                freq_hz = int(f)  # Round to integer for grouping
                if freq_hz not in freq_to_measurements:
                    freq_to_measurements[freq_hz] = {'freq': [], 'z_mag': [], 'phase': []}
                freq_to_measurements[freq_hz]['freq'].append(f)
                freq_to_measurements[freq_hz]['z_mag'].append(z)
                freq_to_measurements[freq_hz]['phase'].append(p)

        # Average each frequency
        freq = []
        z_mag = []
        phase = []

        for freq_hz in sorted(freq_to_measurements.keys()):
            measurements = freq_to_measurements[freq_hz]

            # Average frequency and magnitude (arithmetic mean)
            avg_freq = np.mean(measurements['freq'])
            avg_magnitude = np.mean(measurements['z_mag'])

            # Average phase (circular mean)
            phases = measurements['phase']
            angles_rad = np.radians(phases)
            sin_mean = np.mean(np.sin(angles_rad))
            cos_mean = np.mean(np.cos(angles_rad))
            avg_phase = np.degrees(np.arctan2(sin_mean, cos_mean))

            freq.append(avg_freq)
            z_mag.append(avg_magnitude)
            phase.append(avg_phase)

        return {
            'freq': np.array(freq),
            'z_mag': np.array(z_mag),
            'phase': np.array(phase)
        }

    def parse_biopal_csv(self, filepath):
        """Parse BioPal CSV format"""
        print(f"Loading BioPal CSV: {filepath}")

        freq = []
        z_mag = []
        phase = []
        skipped = 0

        with open(filepath, 'r') as f:
            reader = csv.reader(f)
            header = next(reader)  # Skip header

            for row in reader:
                if len(row) < 4:
                    continue

                try:
                    # DUT, Frequency, Magnitude, Phase
                    f = float(row[1])
                    z = float(row[2])
                    p = float(row[3])

                    # Skip invalid data points
                    if f <= 0 or z <= 0:
                        skipped += 1
                        continue

                    # Normalize phase to -180 to +180 range
                    while p > 180:
                        p -= 360
                    while p < -180:
                        p += 360

                    freq.append(f)
                    z_mag.append(z)
                    phase.append(p)
                except (ValueError, IndexError):
                    skipped += 1
                    continue

        if len(freq) == 0:
            raise ValueError("No valid data found in BioPal CSV")

        print(f"  ✓ Loaded {len(freq)} data points")
        if skipped > 0:
            print(f"  ⚠ Skipped {skipped} invalid data points (freq=0 or magnitude=0)")

        # Print data range for debugging
        print(f"  Frequency range: {min(freq):.1f} Hz to {max(freq):.1f} Hz")
        print(f"  Magnitude range: {min(z_mag):.1f} Ω to {max(z_mag):.1f} Ω")
        print(f"  Phase range: {min(phase):.2f}° to {max(phase):.2f}°")

        return {
            'freq': np.array(freq),
            'z_mag': np.array(z_mag),
            'phase': np.array(phase)
        }

    def compare_and_plot(self, ps_file, bp_file):
        """Load data, compare, and plot results"""
        # Load data
        self.ps_data = self.parse_palmsens_csv(ps_file)
        self.bp_data = self.parse_biopal_csv(bp_file)

        # Find common frequency range
        freq_min = max(self.ps_data['freq'].min(), self.bp_data['freq'].min())
        freq_max = min(self.ps_data['freq'].max(), self.bp_data['freq'].max())

        print(f"\nCommon frequency range: {freq_min:.1f} Hz to {freq_max:.1f} Hz")

        # Use PalmSens frequencies as reference (ground truth)
        freq_common_mask = (self.ps_data['freq'] >= freq_min) & (self.ps_data['freq'] <= freq_max)
        freq_common = self.ps_data['freq'][freq_common_mask]

        if len(freq_common) < 2:
            raise ValueError("Insufficient overlapping frequency range between datasets")

        # Interpolate BioPal data to match PalmSens frequencies
        bp_z_interp = np.interp(freq_common, self.bp_data['freq'], self.bp_data['z_mag'])
        bp_phase_interp = np.interp(freq_common, self.bp_data['freq'], self.bp_data['phase'])

        # Get corresponding PalmSens data
        ps_z = self.ps_data['z_mag'][freq_common_mask]
        ps_phase = self.ps_data['phase'][freq_common_mask]

        # Calculate percentage errors
        z_error = ((bp_z_interp - ps_z) / ps_z) * 100
        phase_error = (bp_phase_interp - ps_phase)

        # Print statistics
        print("\n=== Error Statistics ===")
        print(f"Magnitude Error:")
        print(f"  Mean: {np.mean(z_error):.2f}%")
        print(f"  Std:  {np.std(z_error):.2f}%")
        print(f"  Max:  {np.max(np.abs(z_error)):.2f}%")
        print(f"\nPhase Error:")
        print(f"  Mean: {np.mean(phase_error):.2f} deg")
        print(f"  Std:  {np.std(phase_error):.2f} deg")
        print(f"  Max:  {np.max(np.abs(phase_error)):.2f} deg")

        # Plot
        self.plot_comparison(freq_common, ps_z, ps_phase, bp_z_interp, bp_phase_interp, z_error, phase_error)

    def plot_comparison(self, freq, ps_z, ps_phase, bp_z, bp_phase, z_error, phase_error):
        """Create comparison plots"""
        fig = plt.figure(figsize=(14, 8))
        fig.suptitle('Impedance Comparison: PalmSens vs BioPal', fontsize=16, fontweight='bold')

        # Create 2x2 subplot grid
        # [0,0]: Magnitude comparison
        # [0,1]: Phase comparison
        # [1,0]: Magnitude error
        # [1,1]: Phase error

        # Magnitude comparison
        ax1 = plt.subplot(2, 2, 1)
        ax1.loglog(freq, ps_z, 'b-', label='PalmSens', linewidth=2, marker='o', markersize=4)
        ax1.loglog(freq, bp_z, 'r--', label='BioPal', linewidth=2, marker='s', markersize=3)
        ax1.set_xlabel('Frequency (Hz)', fontsize=11)
        ax1.set_ylabel('|Z| (Ω)', fontsize=11)
        ax1.set_title('Impedance Magnitude Comparison (Log-Log)', fontsize=12, fontweight='bold')
        ax1.grid(True, which='both', alpha=0.3)
        ax1.legend(fontsize=10)

        # Phase comparison
        ax2 = plt.subplot(2, 2, 2)
        ax2.semilogx(freq, ps_phase, 'b-', label='PalmSens', linewidth=2, marker='o', markersize=4)
        ax2.semilogx(freq, bp_phase, 'r--', label='BioPal', linewidth=2, marker='s', markersize=3)
        ax2.set_xlabel('Frequency (Hz)', fontsize=11)
        ax2.set_ylabel('Phase (°)', fontsize=11)
        ax2.set_title('Phase Comparison', fontsize=12, fontweight='bold')
        ax2.grid(True, which='both', alpha=0.3)
        ax2.legend(fontsize=10)

        # Magnitude error
        ax3 = plt.subplot(2, 2, 3)
        ax3.semilogx(freq, z_error, 'g-', linewidth=2, marker='o', markersize=4)
        ax3.axhline(y=0, color='k', linestyle='--', alpha=0.5, linewidth=1)
        ax3.fill_between(freq, 0, z_error, alpha=0.3, color='green')
        ax3.set_xlabel('Frequency (Hz)', fontsize=11)
        ax3.set_ylabel('Error (%)', fontsize=11)
        ax3.set_title('Magnitude Error (BioPal - PalmSens)', fontsize=12, fontweight='bold')
        ax3.grid(True, which='both', alpha=0.3)

        # Add statistics box
        mean_z_err = np.mean(z_error)
        std_z_err = np.std(z_error)
        max_z_err = np.max(np.abs(z_error))
        stats_text = f'Mean: {mean_z_err:.2f}%\nStd: {std_z_err:.2f}%\nMax: {max_z_err:.2f}%'
        ax3.text(0.02, 0.98, stats_text, transform=ax3.transAxes,
                verticalalignment='top', fontsize=9,
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))

        # Phase error
        ax4 = plt.subplot(2, 2, 4)
        ax4.semilogx(freq, phase_error, 'm-', linewidth=2, marker='o', markersize=4)
        ax4.axhline(y=0, color='k', linestyle='--', alpha=0.5, linewidth=1)
        ax4.fill_between(freq, 0, phase_error, alpha=0.3, color='magenta')
        ax4.set_xlabel('Frequency (Hz)', fontsize=11)
        ax4.set_ylabel('Error (deg)', fontsize=11)
        ax4.set_title('Phase Error (BioPal - PalmSens)', fontsize=12, fontweight='bold')
        ax4.grid(True, which='both', alpha=0.3)

        # Add statistics box
        mean_phase_err = np.mean(phase_error)
        std_phase_err = np.std(phase_error)
        max_phase_err = np.max(np.abs(phase_error))
        stats_text = f'Mean: {mean_phase_err:.2f} deg\nStd: {std_phase_err:.2f} deg\nMax: {max_phase_err:.2f} deg'
        ax4.text(0.02, 0.98, stats_text, transform=ax4.transAxes,
                verticalalignment='top', fontsize=9,
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))

        plt.tight_layout()
        plt.show()


def main():
    """Main entry point"""
    print("\n=== Impedance Comparison Tool ===")
    print("PalmSens vs BioPal Analysis\n")

    # Check command line arguments
    if len(sys.argv) == 3:
        ps_file = sys.argv[1]
        bp_file = sys.argv[2]
    else:
        print("No command line arguments provided.")
        print("Opening file dialogues...\n")

        # Create hidden root window for file dialogues
        root = tk.Tk()
        root.withdraw()  # Hide the main window

        # File dialogue for PalmSens CSV
        print("Select PalmSens CSV file...")
        ps_file = filedialog.askopenfilename(
            title="Select PalmSens CSV File",
            filetypes=[
                ("CSV files", "*.csv"),
                ("All files", "*.*")
            ]
        )

        if not ps_file:
            print("No PalmSens file selected, exiting.")
            root.destroy()
            return

        print(f"Selected PalmSens file: {ps_file}")

        # File dialogue for BioPal CSV
        print("\nSelect BioPal CSV file...")
        bp_file = filedialog.askopenfilename(
            title="Select BioPal CSV File",
            filetypes=[
                ("CSV files", "*.csv"),
                ("All files", "*.*")
            ]
        )

        if not bp_file:
            print("No BioPal file selected, exiting.")
            root.destroy()
            return

        print(f"Selected BioPal file: {bp_file}\n")

        root.destroy()

    # Check files exist
    if not os.path.exists(ps_file):
        print(f"ERROR: PalmSens file not found: {ps_file}")
        return

    if not os.path.exists(bp_file):
        print(f"ERROR: BioPal file not found: {bp_file}")
        return

    # Run comparison
    try:
        comp = ImpedanceComparison()
        comp.compare_and_plot(ps_file, bp_file)
    except Exception as e:
        print(f"\nERROR: {e}")
        import traceback
        traceback.print_exc()
        return


if __name__ == "__main__":
    main()
