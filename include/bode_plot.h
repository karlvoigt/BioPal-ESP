#ifndef BODE_PLOT_H
#define BODE_PLOT_H

#include <Arduino.h>

/*=========================BODE PLOT=========================*/

// Draw Bode plot for a specific DUT
// dutIndex: 0-3 (DUT 1-4)
// Shows impedance magnitude (log-log, solid) and phase (semi-log, dashed)
void drawBodePlot(uint8_t dutIndex);

#endif // BODE_PLOT_H
