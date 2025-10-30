#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>
#include "defines.h"

/*=========================CALIBRATION MODE ENUM=========================*/
enum CalibrationMode {
    CALIBRATION_MODE_LOOKUP,        // Use lookup table from calibration.csv
    CALIBRATION_MODE_FORMULA,       // Use quadratic formula with coefficients
    CALIBRATION_MODE_SEPARATE_FILES // Use separate CSV files for voltage, TIA, and PGA
};

/*=========================CALIBRATION COEFFICIENTS STRUCT=========================*/
struct CalibrationCoefficients {
    float m0, m1, m2;  // Magnitude coefficients
    float a1, a2;      // Phase coefficients
    float r_squared_mag;   // R² for magnitude fit
    float r_squared_phase; // R² for phase fit
    bool valid;        // Whether coefficients are loaded

    CalibrationCoefficients() :
        m0(1.0), m1(0.0), m2(0.0),
        a1(0.0), a2(0.0),
        r_squared_mag(0.0), r_squared_phase(0.0),
        valid(false) {}
};

/*=========================CALIBRATION POINT CLASS=========================*/
class CalibrationPoint
{
    private:
    public:
        float impedance_gain; // Impedance gain
        float phase_offset; // Phase offset in degrees

        CalibrationPoint() : impedance_gain(1.0), phase_offset(0.0) {}

        CalibrationPoint(float Z_gain, float phase)
        {
            impedance_gain = Z_gain;
            phase_offset = phase;
        }

        void setCalibrationPoint(float Z_gain, float phase)
        {
            impedance_gain = Z_gain;
            phase_offset = phase;
        }
};

/*=========================NEW CALIBRATION STRUCTURES=========================*/
// Simple calibration point - just gain and phase offset
struct SimpleCalPoint {
    float gain;
    float phase_offset;

    SimpleCalPoint() : gain(1.0), phase_offset(0.0) {}
    SimpleCalPoint(float g, float p) : gain(g), phase_offset(p) {}
};

// Frequency-indexed calibration data
struct FreqCalPoint {
    uint32_t frequency_hz;
    SimpleCalPoint calPoint;

    FreqCalPoint() : frequency_hz(0) {}
    FreqCalPoint(uint32_t freq, float gain, float phase)
        : frequency_hz(freq), calPoint(gain, phase) {}
};

/*=========================FREQ CALIBRATION DATA CLASS=========================*/
class FreqCalibrationData
{
    private:
    public:
        uint32_t frequency_hz; // Frequency in Hz
        CalibrationPoint low_TIA_gains[8]; 
        CalibrationPoint high_TIA_gains[8];

        FreqCalibrationData(uint32_t freq, CalibrationPoint low_gains[8], CalibrationPoint high_gains[8]) {
            frequency_hz = freq;
            for(int i=0; i<8; i++) {
                low_TIA_gains[i] = low_gains[i];
                high_TIA_gains[i] = high_gains[i];
            }
        }

        FreqCalibrationData() : frequency_hz(1000) {
            for(int i=0; i<8; i++) {
                low_TIA_gains[i] = CalibrationPoint();
                high_TIA_gains[i] = CalibrationPoint();
            }
        }

        void setFreqCalibrationData(uint32_t freq, CalibrationPoint low_gains[8], CalibrationPoint high_gains[8]) {
            frequency_hz = freq;
            for(int i=0; i<8; i++) {
                low_TIA_gains[i] = low_gains[i];
                high_TIA_gains[i] = high_gains[i];
            }
        }
};

/*=========================GLOBAL CALIBRATION DATA=========================*/
#define MAX_CAL_FREQUENCIES 38

extern FreqCalibrationData calibrationData[MAX_CAL_FREQUENCIES];
extern int numCalibrationFreqs;

// Calibration coefficients: [TIA_mode][PGA_gain]
// TIA_mode: 0=high (7500Ω), 1=low (37.5Ω)
// PGA_gain: 0-7 (1, 2, 5, 10, 20, 50, 100, 200)
extern CalibrationCoefficients calibrationCoefficients[2][8];

// Current calibration mode
extern CalibrationMode calibrationMode;

/*=========================NEW CALIBRATION DATA ARRAYS=========================*/
// Separate calibration data for voltage, TIA, and PGA
extern FreqCalPoint voltageCalData[MAX_CAL_FREQUENCIES];
extern int numVoltageFreqs;

extern FreqCalPoint tiaHighCalData[MAX_CAL_FREQUENCIES];
extern int numTIAHighFreqs;

extern FreqCalPoint tiaLowCalData[MAX_CAL_FREQUENCIES];
extern int numTIALowFreqs;

// PGA calibration: [PGA_gain_index][frequency]
// PGA_gain_index: 0-7 (1, 2, 5, 10, 20, 50, 100, 200)
extern FreqCalPoint pgaCalData[8][MAX_CAL_FREQUENCIES];
extern int numPGAFreqs[8];

// PS Trace calibration: Final calibration step to match PalmSens reference
extern FreqCalPoint psTraceCalData[MAX_CAL_FREQUENCIES];
extern int numPSTraceFreqs;

// Calibration arrays
// extern float v_phase_shifts[MAX_CAL_FREQUENCIES];
// extern float v_gain[MAX_CAL_FREQUENCIES];
// extern float I_low_phase_shift[MAX_CAL_FREQUENCIES];
// extern float I_low_gain[MAX_CAL_FREQUENCIES];
// extern float I_high_phase_shift[MAX_CAL_FREQUENCIES];
// extern float I_high_gain[MAX_CAL_FREQUENCIES];

/*=========================CALIBRATION FUNCTIONS=========================*/

// Load calibration data from filesystem (/calibration.csv)
// Returns true on success, false on failure
bool loadCalibrationData();

// Load calibration coefficients from filesystem (/calibration_coefficients.csv)
// Returns true on success, false on failure
bool loadCalibrationCoefficients();

// Get calibration point for specific frequency and gain settings
// Returns pointer to CalibrationPoint or nullptr if not found
CalibrationPoint* getCalibrationPoint(uint32_t freq, bool lowTIA, uint8_t pgaGain);

// Find the index of a frequency in the calibration data
// Returns -1 if not found
int findFrequencyIndex(uint32_t freq);

// Apply calibration using quadratic formula
// Formula: |Z_x| = |Z_nc| / (m0 + m1*f + m2*f²)
//          arg(Z_x) = arg(Z_nc) - (a1*f + a2*f²)
bool calibrateWithFormula(ImpedancePoint& point);

// Apply calibration to measured voltage, current, and phase
// Uses current calibrationMode to select method
bool calibrate(ImpedancePoint& point);

// Set calibration mode
void setCalibrationMode(CalibrationMode mode);

// Get current calibration mode
CalibrationMode getCalibrationMode();

/*=========================NEW CALIBRATION FUNCTIONS=========================*/

// Load new separate calibration files
// Returns true on success, false on failure
bool loadVoltageCalibration();  // Loads /voltage.csv
bool loadTIACalibration();      // Loads /tia_high.csv and /tia_low.csv
bool loadPGACalibration();      // Loads /pga_1.csv through /pga_200.csv

// Load all new calibration files
bool loadSeparateCalibrationFiles();

// Get individual calibration values for a specific frequency and settings
// Returns pointers to SimpleCalPoint or nullptr if not found
SimpleCalPoint* getVoltageCalPoint(uint32_t freq);
SimpleCalPoint* getTIACalPoint(uint32_t freq, bool lowTIA);
SimpleCalPoint* getPGACalPoint(uint32_t freq, uint8_t pgaGain);

// Apply new calibration formula
// Formula: mag = (uncalibrated / v_gain) * tia_gain * pga_gain
//          phase = uncalibrated_phase - v_phase + tia_phase + pga_phase
bool calibrateWithSeparateFiles(ImpedancePoint& point);

/*=========================PS TRACE CALIBRATION FUNCTIONS=========================*/

// Load PS Trace calibration from /data/ps_trace.csv
// Format: freq_hz,mag_ratio,phase_offset
// Returns true on success, false on failure
bool loadPSTraceCalibration();

// Apply PS Trace calibration as final step
// mag_final = mag_calibrated * mag_ratio
// phase_final = phase_calibrated + phase_offset
void applyPSTraceCalibration(ImpedancePoint& point);

#endif // CALIBRATION_H
