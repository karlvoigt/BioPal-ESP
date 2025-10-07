#include "calibration.h"
#include <math.h>

// Calibration table - to be populated with actual calibration data
// Format: {frequency_hz, voltage_gain_V_per_V, current_gain_V_per_mA, phase_shift_deg}
static const CalibrationPoint calibration_table[] = {
    // TODO: Replace with actual calibration data from calibration_tool.py
    {        1, 1.0f, 1.0f, 0.0f},
    {        2, 1.0f, 1.0f, 0.0f},
    {        4, 1.0f, 1.0f, 0.0f},
    {        5, 1.0f, 1.0f, 0.0f},
    {        8, 1.0f, 1.0f, 0.0f},
    {       10, 1.0f, 1.0f, 0.0f},
    {       16, 1.0f, 1.0f, 0.0f},
    {       20, 1.0f, 1.0f, 0.0f},
    {       25, 1.0f, 1.0f, 0.0f},
    {       32, 1.0f, 1.0f, 0.0f},
    {       40, 1.0f, 1.0f, 0.0f},
    {       50, 1.0f, 1.0f, 0.0f},
    {       80, 1.0f, 1.0f, 0.0f},
    {      100, 1.0f, 1.0f, 0.0f},
    {      125, 1.0f, 1.0f, 0.0f},
    {      160, 1.0f, 1.0f, 0.0f},
    {      200, 1.0f, 1.0f, 0.0f},
    {      250, 1.0f, 1.0f, 0.0f},
    {      400, 1.0f, 1.0f, 0.0f},
    {      500, 1.0f, 1.0f, 0.0f},
    {      625, 1.0f, 1.0f, 0.0f},
    {      800, 1.0f, 1.0f, 0.0f},
    {     1000, 1.0f, 1.0f, 0.0f},
    {     1250, 1.0f, 1.0f, 0.0f},
    {     2000, 1.0f, 1.0f, 0.0f},
    {     2500, 1.0f, 1.0f, 0.0f},
    {     3125, 1.0f, 1.0f, 0.0f},
    {     4000, 1.0f, 1.0f, 0.0f},
    {     5000, 1.0f, 1.0f, 0.0f},
    {     6250, 1.0f, 1.0f, 0.0f},
    {    10000, 1.0f, 1.0f, 0.0f},
    {    12500, 1.0f, 1.0f, 0.0f},
    {    15625, 1.0f, 1.0f, 0.0f},
    {    25000, 1.0f, 1.0f, 0.0f},
    {    50000, 1.0f, 1.0f, 0.0f},
    {    62500, 1.0f, 1.0f, 0.0f},
    {    80000, 1.0f, 1.0f, 0.0f},
    {   100000, 1.0f, 1.0f, 0.0f},
};

static const uint8_t calibration_table_size = sizeof(calibration_table) / sizeof(CalibrationPoint);

Calibration::Calibration() {
}

bool Calibration::begin() {
    Serial.printf("Calibration: Loaded %d frequency points\n", calibration_table_size);
    return true;
}

uint8_t Calibration::getCalibrationCount() {
    return calibration_table_size;
}

float Calibration::interpolate(float x, float x1, float y1, float x2, float y2) {
    if (x2 == x1) return y1;
    return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}

bool Calibration::findBracketingPoints(uint32_t freq_hz, uint8_t& lower_idx, uint8_t& upper_idx) {
    // Handle edge cases
    if (freq_hz <= calibration_table[0].frequency_hz) {
        lower_idx = 0;
        upper_idx = 0;
        return true;
    }

    if (freq_hz >= calibration_table[calibration_table_size - 1].frequency_hz) {
        lower_idx = calibration_table_size - 1;
        upper_idx = calibration_table_size - 1;
        return true;
    }

    // Find bracketing points
    for (uint8_t i = 0; i < calibration_table_size - 1; i++) {
        if (freq_hz >= calibration_table[i].frequency_hz &&
            freq_hz <= calibration_table[i + 1].frequency_hz) {
            lower_idx = i;
            upper_idx = i + 1;
            return true;
        }
    }

    return false;
}

bool Calibration::getCalibration(uint32_t freq_hz, float& voltage_gain, float& current_gain, float& phase_shift) {
    uint8_t lower_idx, upper_idx;

    if (!findBracketingPoints(freq_hz, lower_idx, upper_idx)) {
        Serial.printf("Calibration: Failed to find bracketing points for %lu Hz\n", freq_hz);
        return false;
    }

    // Exact match or edge case
    if (lower_idx == upper_idx) {
        voltage_gain = calibration_table[lower_idx].voltage_gain;
        current_gain = calibration_table[lower_idx].current_gain;
        phase_shift = calibration_table[lower_idx].phase_shift_deg;
        return true;
    }

    // Linear interpolation on log scale for frequency-dependent gains
    float log_freq = log10f((float)freq_hz);
    float log_f1 = log10f((float)calibration_table[lower_idx].frequency_hz);
    float log_f2 = log10f((float)calibration_table[upper_idx].frequency_hz);

    voltage_gain = interpolate(log_freq, log_f1,
                               calibration_table[lower_idx].voltage_gain,
                               log_f2,
                               calibration_table[upper_idx].voltage_gain);

    current_gain = interpolate(log_freq, log_f1,
                               calibration_table[lower_idx].current_gain,
                               log_f2,
                               calibration_table[upper_idx].current_gain);

    phase_shift = interpolate(log_freq, log_f1,
                              calibration_table[lower_idx].phase_shift_deg,
                              log_f2,
                              calibration_table[upper_idx].phase_shift_deg);

    return true;
}

void Calibration::applyCalibration(uint32_t freq_hz, float& voltage, float& current, float& phase) {
    float voltage_gain, current_gain, phase_shift;

    if (getCalibration(freq_hz, voltage_gain, current_gain, phase_shift)) {
        // Apply gain corrections
        voltage = voltage / voltage_gain;
        current = current / current_gain;

        // Apply phase shift compensation
        phase = phase - phase_shift;

        // Normalize phase to [-180, 180]
        while (phase > 180.0f) phase -= 360.0f;
        while (phase < -180.0f) phase += 360.0f;
    } else {
        Serial.printf("Calibration: Failed to calibrate %lu Hz\n", freq_hz);
    }
}

void Calibration::printCalibrationTable() {
    Serial.println("\n=== CALIBRATION TABLE ===");
    Serial.println("Freq(Hz) | V_Gain | I_Gain | Phase(deg)");
    Serial.println("---------|--------|--------|----------");

    for (uint8_t i = 0; i < calibration_table_size; i++) {
        const CalibrationPoint* point = &calibration_table[i];
        Serial.printf("%8lu | %6.3f | %6.3f | %6.2f\n",
                      point->frequency_hz,
                      point->voltage_gain,
                      point->current_gain,
                      point->phase_shift_deg);
    }
    Serial.println();
}
