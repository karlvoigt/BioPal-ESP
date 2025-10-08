#ifndef DEFINES_H
#define DEFINES_H

#define MAX_DUT_COUNT 3

#include <Arduino.h>

struct MeasurementPoint {
    uint32_t freq_hz;
    float V_magnitude;  // Voltage magnitude
    float I_magnitude;  // Current magnitude
    float phase_deg;  // Phase in degrees
    uint8_t pga_gain;  // PGA gain setting
    bool tia_gain;     // TIA gain setting (true=low, false=high)
    bool valid;       // Validity flag
};

#endif // DEFINES_H