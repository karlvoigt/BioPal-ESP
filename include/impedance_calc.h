#ifndef IMPEDANCE_CALC_H
#define IMPEDANCE_CALC_H

#include <Arduino.h>
#include <vector>
#include "uart_types.h"
#include "calibration.h"

struct ImpedancePoint {
    uint32_t freq_hz;
    float magnitude_ohm;     // |Z| in ohms
    float phase_deg;         // Phase angle in degrees
    bool valid;
};


#endif // IMPEDANCE_CALC_H
