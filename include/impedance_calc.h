#ifndef IMPEDANCE_CALC_H
#define IMPEDANCE_CALC_H

#include <Arduino.h>
#include "defines.h"

ImpedancePoint calcImpedance(MeasurementPoint measPoint);

void calculateRiskLevel(uint8_t dutIdx, uint32_t freqStartHz, uint32_t freqEndHz);


#endif // IMPEDANCE_CALC_H
