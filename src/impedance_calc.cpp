#include "impedance_calc.h"
#include <math.h>

RiskLevel riskLevels[MAX_DUT_COUNT];
float riskPercentages[MAX_DUT_COUNT];
float lowRiskCutoff = 0.05f;  
float mediumRiskCutoff = 0.15f;
float highRiskCutoff = 0.25f;


/*=========================IMPEDANCE CALCULATION=========================*/
// Calculate impedance from voltage and current samples


ImpedancePoint calcImpedance(MeasurementPoint measPoint) {
    ImpedancePoint result;

    if (measPoint.I_magnitude <= 0.0f || !measPoint.valid) {
        // Invalid current or measurement
        result.valid = false;
        return result;
    }

    result.freq_hz = measPoint.freq_hz;
    result.valid = measPoint.valid;
    result.Z_magnitude = measPoint.V_magnitude / measPoint.I_magnitude; // |Z| = V/I
    result.Z_phase = measPoint.phase_deg; // Phase angle already in degrees
    result.pga_gain = measPoint.pga_gain;
    result.tia_gain = measPoint.tia_gain;

    // Print raw measurement m=point data
    Serial.printf("Measurement: freq= %d, V=%.2f, I=%.2f, phase=%.2f, PGA=%d, TIA=%d, valid=%d\n",
                  measPoint.freq_hz, measPoint.V_magnitude, measPoint.I_magnitude,
                  measPoint.phase_deg, measPoint.pga_gain, measPoint.tia_gain, measPoint.valid);

    Serial.printf("Uncalibrated Impedance: freq= %d, |Z|=%.2f, phase=%.2f\n",
                  result.freq_hz, result.Z_magnitude, result.Z_phase);
    return result;
}

// Calculate risk level across range of interest for specified DUT
void calculateRiskLevel(uint8_t dutIdx, uint32_t freqStartHz, uint32_t freqEndHz) {
    // For the DUT, calculate average impedance magnitude change between baseline and final in the specified frequency range
    if (dutIdx >= MAX_DUT_COUNT) {
        riskLevels[dutIdx] = RISK_ERROR; // Invalid DUT index
        riskPercentages[dutIdx] = 0.0f;
        Serial.printf("ERROR: Invalid DUT index %d for risk calculation\n", dutIdx + 1);
        return;
    }
    // Get total change in range of interest
    float totalChange = 0.0f;
    int count = 0;
    for (int i = 0; i < frequencyCount[dutIdx]; i++) {
        ImpedancePoint baselinePoint = baselineImpedanceData[dutIdx][i];
        ImpedancePoint finalPoint = measurementImpedanceData[dutIdx][i];

        if (!baselinePoint.valid || !finalPoint.valid || baselinePoint.Z_magnitude <= 0.0f) {
            continue; // Skip invalid points
        }

        if (baselinePoint.freq_hz >= freqStartHz && baselinePoint.freq_hz <= freqEndHz) {
            float change = fabs(finalPoint.Z_magnitude / baselinePoint.Z_magnitude);
            totalChange += change;
            count++;
        }
    }
    if (count == 0) {
        riskLevels[dutIdx] = RISK_ERROR; // No valid data points in the range of interest
        riskPercentages[dutIdx] = 0.0f;
        Serial.printf("ERROR: No valid data points for DUT %d in frequency range %lu-%lu Hz\n",
                      dutIdx + 1, freqStartHz, freqEndHz);
        return;
    }
    float avgChange = 1.0f - (totalChange / count); //Invert to make % reduction instead of % increase
    
    // Determine risk level based on average change
    if (avgChange < lowRiskCutoff) {
        riskLevels[dutIdx] = RISK_NONE;
    } else if (avgChange < mediumRiskCutoff) {
        riskLevels[dutIdx] = RISK_LOW;
    } else if (avgChange < highRiskCutoff) {
        riskLevels[dutIdx] = RISK_MEDIUM;
    } else if (avgChange >= highRiskCutoff) {
        riskLevels[dutIdx] = RISK_HIGH;
    } else {
        riskLevels[dutIdx] = RISK_ERROR; // Unable to calculate risk
        riskPercentages[dutIdx] = 0.0f;
    }

    riskPercentages[dutIdx] = avgChange*100.0f; // Store as percentage

    // Print risk level
    Serial.printf("DUT %d Risk Calculation: Avg Change=%.3f, Risk Level=%d\n",
                  dutIdx + 1, riskPercentages[dutIdx], riskLevels[dutIdx]);
}
