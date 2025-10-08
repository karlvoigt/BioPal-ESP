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
    result.Z_magnitude = measPoint.V_magnitude / measPoint.I_magnitude * 1000; // |Z| = V/I (I in mA)
    result.Z_phase = measPoint.phase_deg; // Phase angle already in degrees


    Serial.printf("Impedance: freq= %d, |Z|=%.2f, phase=%.2f\n",
                  result.freq_hz, result.Z_magnitude, result.Z_phase);
    return result;
}