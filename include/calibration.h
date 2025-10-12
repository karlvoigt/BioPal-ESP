#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>
#include "defines.h"

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

// Get calibration point for specific frequency and gain settings
// Returns pointer to CalibrationPoint or nullptr if not found
CalibrationPoint* getCalibrationPoint(uint32_t freq, bool lowTIA, uint8_t pgaGain);

// Find the index of a frequency in the calibration data
// Returns -1 if not found
int findFrequencyIndex(uint32_t freq);

// Apply calibration to measured voltage, current, and phase
bool calibrate(ImpedancePoint& point);

#endif // CALIBRATION_H
