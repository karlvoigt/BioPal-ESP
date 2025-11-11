#ifndef DEFINES_H
#define DEFINES_H

#include <Arduino.h>

// System configuration
#define MAX_DUT_COUNT 4        // Number of DUTs (Device Under Test)
#define MAX_FREQUENCIES 38     // Maximum number of frequency points per sweep

// Measurement point from STM32
struct MeasurementPoint {
    uint32_t freq_hz;
    float V_magnitude;  // Voltage magnitude
    float I_magnitude;  // Current magnitude
    float phase_deg;    // Phase in degrees (V-I phase difference)
    uint8_t pga_gain;   // PGA gain setting (0-7)
    bool tia_gain;      // TIA gain setting (true=high, false=low)
    bool valid;         // Validity flag
};

// Calculated impedance point
struct ImpedancePoint {
    uint32_t freq_hz;       // Frequency in Hz
    float Z_magnitude;      // Impedance magnitude in Ohms
    float Z_phase;          // Impedance phase in degrees
    uint8_t pga_gain;       // PGA gain setting (0-7)
    bool tia_gain;          // TIA gain setting (true=high, false=low)
    bool valid;             // Validity flag

    ImpedancePoint() : freq_hz(0), Z_magnitude(0.0), Z_phase(0.0),pga_gain(0),tia_gain(false), valid(false) {}
};

// Risk level for qualitative results
enum RiskLevel {
    RISK_NONE,
    RISK_LOW,
    RISK_MEDIUM,
    RISK_HIGH,
    RISK_ERROR
};

// Risk lvel cutoffs for qualitative analysis
extern float lowRiskCutoff;  
extern float mediumRiskCutoff;
extern float highRiskCutoff;

extern RiskLevel riskLevels[MAX_DUT_COUNT];
extern float riskPercentages[MAX_DUT_COUNT];

extern bool measurementInProgress;
extern bool baselineMeasurementDone;
extern bool finalMeasurementDone;
// Global impedance data storage [DUT][frequency]
extern ImpedancePoint baselineImpedanceData[MAX_DUT_COUNT][MAX_FREQUENCIES];
extern ImpedancePoint measurementImpedanceData[MAX_DUT_COUNT][MAX_FREQUENCIES];

extern int frequencyCount[MAX_DUT_COUNT];  // Number of valid frequencies per DUT

#endif // DEFINES_H