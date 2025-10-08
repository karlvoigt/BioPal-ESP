#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>
#include "defines.h"

/*=========================CALIBRATION POINT CLASS=========================*/
class CalibrationPoint
{
    private:
    public:
        float voltage_gain; // V gain
        float current_gain; // I gain
        float phase_offset; // Phase offset in degrees

        CalibrationPoint() : voltage_gain(1.0), current_gain(1.0), phase_offset(0.0) {}
        
        CalibrationPoint(float V_gain, float I_gain, float phase)
        {
            voltage_gain = V_gain;
            current_gain = I_gain;
            phase_offset = phase;
        }

        void setCalibrationPoint(float V_gain, float I_gain, float phase)
        {
            voltage_gain = V_gain;
            current_gain = I_gain;
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
#define MAX_CAL_FREQUENCIES 50

extern FreqCalibrationData calibrationData[MAX_CAL_FREQUENCIES];
extern int numCalibrationFreqs;

/*=========================CALIBRATION FUNCTIONS=========================*/

// Load calibration data from filesystem (/calibration.csv)
// Returns true on success, false on failure
bool loadCalibrationData();

// Get calibration point for specific frequency and gain settings
// Returns pointer to CalibrationPoint or nullptr if not found
CalibrationPoint* getCalibrationPoint(uint32_t freq, bool highTIA, uint8_t pgaGain);

// Find the index of a frequency in the calibration data
// Returns -1 if not found
int findFrequencyIndex(uint32_t freq);

// Apply calibration to measured voltage, current, and phase
bool calibrate(MeasurementPoint& point);

#endif // CALIBRATION_H
