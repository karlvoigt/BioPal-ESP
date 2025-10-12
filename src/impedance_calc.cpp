#include "impedance_calc.h"
#include <math.h>

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
