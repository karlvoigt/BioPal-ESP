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

class ImpedanceComparison:
    def __init__(self):
        self.ps_data = None
        self.bp_data = None

    def parse_palmsens_csv(self, filepath):
        """Parse PalmSens PS Trace CSV format"""
        print(f"Loading PalmSens CSV: {filepath}")

        # Try different encodings (UTF-8 first as it's most common for CSV)
        for encoding in ['utf-8', 'latin-1', 'utf-16-le', 'utf-16']:
            try:
                with open(filepath, 'r', encoding=encoding) as f:
                    lines = f.readlines()
                print(f"  Successfully decoded with {encoding} encoding")
                break
            except UnicodeDecodeError:
                continue
        else:
            raise ValueError("Could not decode CSV file with any known encoding")

        # Find the data start (header line with "freq")
        data_start = 0
        for i, line in enumerate(lines):
            if 'freq' in line.lower() and 'hz' in line.lower():
                print(f"  Found header at line {i+1}: {line.strip()[:60]}...")
                data_start = i + 1
                break

        if data_start == 0:
            print("  ERROR: Could not find header line containing 'freq' and 'hz'")
            print(f"  First 10 lines of file:")
            for i, line in enumerate(lines[:10]):
                print(f"    Line {i+1}: {line.strip()[:80]}")
            raise ValueError("Could not find data header in PalmSens CSV")

        # Parse data rows
        freq = []
        z_mag = []
        phase = []

        for line in lines[data_start:]:
            line = line.strip()
            if not line or line.startswith('�'):
                continue

            parts = [p.strip() for p in line.split(',')]
            if len(parts) < 4:
                continue

            try:
                # Column 0: Frequency
                # Column 1: Phase (negative)
                # Column 3: Z magnitude
                f = float(parts[0])
                p = float(parts[1])
                z = float(parts[3])

                freq.append(f)
                phase.append(-p)
                z_mag.append(z)
            except (ValueError, IndexError):
                continue

        if len(freq) == 0:
            raise ValueError("No valid data found in PalmSens CSV")

        print(f"  ✓ Loaded {len(freq)} data points")

        # Print data range for debugging
        print(f"  Frequency range: {min(freq):.1f} Hz to {max(freq):.1f} Hz")
        print(f"  Magnitude range: {min(z_mag):.1f} Ω to {max(z_mag):.1f} Ω")
        print(f"  Phase range: {min(phase):.2f}° to {max(phase):.2f}°")

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
        phase_error = ((bp_phase_interp - ps_phase) / ps_phase) * 100

        # Print statistics
        print("\n=== Error Statistics ===")
        print(f"Magnitude Error:")
        print(f"  Mean: {np.mean(z_error):.2f}%")
        print(f"  Std:  {np.std(z_error):.2f}%")
        print(f"  Max:  {np.max(np.abs(z_error)):.2f}%")
        print(f"\nPhase Error:")
        print(f"  Mean: {np.mean(phase_error):.2f}%")
        print(f"  Std:  {np.std(phase_error):.2f}%")
        print(f"  Max:  {np.max(np.abs(phase_error)):.2f}%")

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
        ax4.set_ylabel('Error (%)', fontsize=11)
        ax4.set_title('Phase Error (BioPal - PalmSens)', fontsize=12, fontweight='bold')
        ax4.grid(True, which='both', alpha=0.3)

        # Add statistics box
        mean_phase_err = np.mean(phase_error)
        std_phase_err = np.std(phase_error)
        max_phase_err = np.max(np.abs(phase_error))
        stats_text = f'Mean: {mean_phase_err:.2f}%\nStd: {std_phase_err:.2f}%\nMax: {max_phase_err:.2f}%'
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
        print("Usage: python impedance_comparison_gui.py <palmsens_csv> <biopal_csv>")
        print("\nOr run without arguments for interactive mode:\n")

        # Interactive mode - ask for filenames
        ps_file = input("Enter PalmSens CSV file path: ").strip()
        if not ps_file:
            print("No file specified, exiting.")
            return

        bp_file = input("Enter BioPal CSV file path: ").strip()
        if not bp_file:
            print("No file specified, exiting.")
            return

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
