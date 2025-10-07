#include "impedance_calc.h"
#include <math.h>

ImpedanceCalculator::ImpedanceCalculator(Calibration* cal) : calibration(cal) {
}

void ImpedanceCalculator::polarToComplex(float magnitude, float phase_deg, float& real, float& imag) {
    float phase_rad = phase_deg * M_PI / 180.0f;
    real = magnitude * cosf(phase_rad);
    imag = magnitude * sinf(phase_rad);
}

void ImpedanceCalculator::complexToPolar(float real, float imag, float& magnitude, float& phase_deg) {
    magnitude = sqrtf(real * real + imag * imag);
    phase_deg = atan2f(imag, real) * 180.0f / M_PI;
}

bool ImpedanceCalculator::calculateImpedance(const DUTResults& results, std::vector<ImpedancePoint>& impedance) {
    if (!results.valid) {
        Serial.println("ImpedanceCalc: Invalid DUT results");
        return false;
    }

    if (results.voltage.size() != results.current.size()) {
        Serial.printf("ImpedanceCalc: Voltage/current size mismatch (%d vs %d)\n",
                      results.voltage.size(), results.current.size());
        return false;
    }

    impedance.clear();
    impedance_results.clear();

    for (size_t i = 0; i < results.voltage.size(); i++) {
        const MeasurementPoint& v_meas = results.voltage[i];
        const MeasurementPoint& i_meas = results.current[i];

        ImpedancePoint z_point;
        z_point.freq_hz = v_meas.freq_hz;
        z_point.valid = v_meas.valid && i_meas.valid;

        if (!z_point.valid) {
            Serial.printf("ImpedanceCalc: Invalid measurement at %lu Hz\n", z_point.freq_hz);
            z_point.magnitude_ohm = 0.0f;
            z_point.phase_deg = 0.0f;
            impedance.push_back(z_point);
            continue;
        }

        // Apply calibration
        float v_mag = v_meas.magnitude;
        float i_mag = i_meas.magnitude;
        float v_phase = v_meas.phase_deg;
        float i_phase = i_meas.phase_deg;

        if (calibration) {
            calibration->applyCalibration(z_point.freq_hz, v_mag, i_mag, v_phase);
            calibration->applyCalibration(z_point.freq_hz, i_mag, i_mag, i_phase);
        }

        // Convert voltage and current to complex form
        float v_real, v_imag, i_real, i_imag;
        polarToComplex(v_mag, v_phase, v_real, v_imag);
        polarToComplex(i_mag, i_phase, i_real, i_imag);

        // Calculate impedance: Z = V / I (complex division)
        // Z = (V_real + jV_imag) / (I_real + jI_imag)
        // Z = (V_real + jV_imag) * (I_real - jI_imag) / (I_real^2 + I_imag^2)

        float i_mag_sq = i_real * i_real + i_imag * i_imag;

        if (i_mag_sq < 1e-9f) {
            Serial.printf("ImpedanceCalc: Current too small at %lu Hz\n", z_point.freq_hz);
            z_point.valid = false;
            z_point.magnitude_ohm = 0.0f;
            z_point.phase_deg = 0.0f;
            impedance.push_back(z_point);
            continue;
        }

        float z_real = (v_real * i_real + v_imag * i_imag) / i_mag_sq;
        float z_imag = (v_imag * i_real - v_real * i_imag) / i_mag_sq;

        // Convert back to polar form
        complexToPolar(z_real, z_imag, z_point.magnitude_ohm, z_point.phase_deg);

        impedance.push_back(z_point);
    }

    impedance_results = impedance;
    Serial.printf("ImpedanceCalc: Calculated %d impedance points for DUT %d\n",
                  impedance.size(), results.dut_number);

    return true;
}

void ImpedanceCalculator::printImpedance() {
    Serial.println("\n=== IMPEDANCE RESULTS ===");
    Serial.println("Freq(Hz) | |Z|(ohm) | Phase(deg) | Valid");
    Serial.println("---------|----------|------------|------");

    for (const auto& point : impedance_results) {
        Serial.printf("%8lu | %8.2f | %10.2f | %s\n",
                      point.freq_hz,
                      point.magnitude_ohm,
                      point.phase_deg,
                      point.valid ? "YES" : "NO");
    }
    Serial.println();
}
