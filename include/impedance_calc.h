#ifndef IMPEDANCE_CALC_H
#define IMPEDANCE_CALC_H

#include <Arduino.h>
#include <vector>
#include "uart_stm32.h"
#include "calibration.h"

struct ImpedancePoint {
    uint32_t freq_hz;
    float magnitude_ohm;     // |Z| in ohms
    float phase_deg;         // Phase angle in degrees
    bool valid;
};

class ImpedanceCalculator {
public:
    ImpedanceCalculator(Calibration* cal);

    // Calculate impedance from voltage and current measurements
    bool calculateImpedance(const DUTResults& results, std::vector<ImpedancePoint>& impedance);

    // Get latest impedance results
    const std::vector<ImpedancePoint>& getImpedance() { return impedance_results; }

    // Print impedance results
    void printImpedance();

private:
    Calibration* calibration;
    std::vector<ImpedancePoint> impedance_results;

    // Convert magnitude and phase to complex number
    void polarToComplex(float magnitude, float phase_deg, float& real, float& imag);

    // Convert complex number to magnitude and phase
    void complexToPolar(float real, float imag, float& magnitude, float& phase_deg);
};

#endif // IMPEDANCE_CALC_H
