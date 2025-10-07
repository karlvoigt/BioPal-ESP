#ifndef BODE_PLOT_H
#define BODE_PLOT_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <vector>
#include "impedance_calc.h"

class BodePlot {
public:
    BodePlot(TFT_eSPI* display);
    bool begin();

    // Plot impedance data (magnitude and phase)
    void plotImpedance(const std::vector<ImpedancePoint>& impedance, uint8_t dut_number);

    // Clear the display
    void clear();

    // Display status message
    void displayStatus(const char* message);

private:
    TFT_eSPI* tft;

    // Plot dimensions
    static const uint16_t PLOT_MARGIN_LEFT = 50;
    static const uint16_t PLOT_MARGIN_RIGHT = 10;
    static const uint16_t PLOT_MARGIN_TOP = 30;
    static const uint16_t PLOT_MARGIN_BOTTOM = 40;
    static const uint16_t PLOT_SPACING = 20;

    uint16_t screen_width;
    uint16_t screen_height;
    uint16_t plot_width;
    uint16_t plot_height;

    // Plot individual Bode diagram (magnitude or phase)
    void plotMagnitude(const std::vector<ImpedancePoint>& impedance, uint16_t y_offset);
    void plotPhase(const std::vector<ImpedancePoint>& impedance, uint16_t y_offset);

    // Drawing helpers
    void drawAxes(uint16_t x_start, uint16_t y_start, uint16_t width, uint16_t height,
                  const char* title, const char* y_label);
    void drawGrid(uint16_t x_start, uint16_t y_start, uint16_t width, uint16_t height,
                  uint8_t x_divisions, uint8_t y_divisions);

    // Coordinate mapping
    float mapLogX(uint32_t freq_hz, uint32_t min_freq, uint32_t max_freq, uint16_t plot_width);
    float mapLinearY(float value, float min_val, float max_val, uint16_t plot_height);

    // Find data range
    void findFrequencyRange(const std::vector<ImpedancePoint>& impedance, uint32_t& min_freq, uint32_t& max_freq);
    void findMagnitudeRange(const std::vector<ImpedancePoint>& impedance, float& min_mag, float& max_mag);
    void findPhaseRange(const std::vector<ImpedancePoint>& impedance, float& min_phase, float& max_phase);
};

#endif // BODE_PLOT_H
