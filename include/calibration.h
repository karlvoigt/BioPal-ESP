#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>

struct CalibrationPoint {
    uint32_t frequency_hz;
    float voltage_gain;      // V/V (measured/actual)
    float current_gain;      // V/mA (measured/actual)
    float phase_shift_deg;   // Phase compensation (degrees)
};

class Calibration {
public:
    Calibration();
    bool begin();

    // Get calibration values for a specific frequency
    bool getCalibration(uint32_t freq_hz, float& voltage_gain, float& current_gain, float& phase_shift);

    // Apply calibration to raw measurements
    void applyCalibration(uint32_t freq_hz, float& voltage, float& current, float& phase);

    // Get number of calibration points
    uint8_t getCalibrationCount();

    // Print calibration table for debugging
    void printCalibrationTable();

private:
    // Linear interpolation between two calibration points
    float interpolate(float x, float x1, float y1, float x2, float y2);

    // Find calibration points bracketing the target frequency
    bool findBracketingPoints(uint32_t freq_hz, uint8_t& lower_idx, uint8_t& upper_idx);
};

#endif // CALIBRATION_H
