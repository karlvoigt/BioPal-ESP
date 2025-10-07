#include "bode_plot.h"
#include <math.h>

BodePlot::BodePlot(TFT_eSPI* display) : tft(display) {
}

bool BodePlot::begin() {
    screen_width = tft->width();
    screen_height = tft->height();

    // Calculate available space for each plot
    uint16_t total_plot_height = screen_height - PLOT_MARGIN_TOP - PLOT_MARGIN_BOTTOM - PLOT_SPACING;
    plot_height = total_plot_height / 2;
    plot_width = screen_width - PLOT_MARGIN_LEFT - PLOT_MARGIN_RIGHT;

    Serial.printf("BodePlot: Initialized (%dx%d screen, %dx%d plots)\n",
                  screen_width, screen_height, plot_width, plot_height);

    clear();
    return true;
}

void BodePlot::clear() {
    tft->fillScreen(TFT_BLACK);
}

void BodePlot::displayStatus(const char* message) {
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextSize(2);
    tft->setCursor(10, screen_height / 2);
    tft->println(message);
}

void BodePlot::plotImpedance(const std::vector<ImpedancePoint>& impedance, uint8_t dut_number) {
    if (impedance.empty()) {
        displayStatus("No data to plot");
        return;
    }

    clear();

    // Draw title
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextSize(2);
    tft->setCursor(10, 5);
    tft->printf("DUT %d Impedance", dut_number);

    // Plot magnitude on top half
    uint16_t mag_y_offset = PLOT_MARGIN_TOP;
    plotMagnitude(impedance, mag_y_offset);

    // Plot phase on bottom half
    uint16_t phase_y_offset = PLOT_MARGIN_TOP + plot_height + PLOT_SPACING;
    plotPhase(impedance, phase_y_offset);

    Serial.printf("BodePlot: Plotted %d points for DUT %d\n", impedance.size(), dut_number);
}

void BodePlot::plotMagnitude(const std::vector<ImpedancePoint>& impedance, uint16_t y_offset) {
    uint32_t min_freq, max_freq;
    float min_mag, max_mag;

    findFrequencyRange(impedance, min_freq, max_freq);
    findMagnitudeRange(impedance, min_mag, max_mag);

    // Add 10% margin to magnitude range
    float mag_range = max_mag - min_mag;
    min_mag -= mag_range * 0.1f;
    max_mag += mag_range * 0.1f;

    // Draw axes and grid
    drawAxes(PLOT_MARGIN_LEFT, y_offset, plot_width, plot_height, "|Z| (ohm)", "");
    drawGrid(PLOT_MARGIN_LEFT, y_offset, plot_width, plot_height, 5, 4);

    // Plot data points
    for (size_t i = 1; i < impedance.size(); i++) {
        if (!impedance[i - 1].valid || !impedance[i].valid) continue;

        float x1 = PLOT_MARGIN_LEFT + mapLogX(impedance[i - 1].freq_hz, min_freq, max_freq, plot_width);
        float y1 = y_offset + plot_height - mapLinearY(impedance[i - 1].magnitude_ohm, min_mag, max_mag, plot_height);

        float x2 = PLOT_MARGIN_LEFT + mapLogX(impedance[i].freq_hz, min_freq, max_freq, plot_width);
        float y2 = y_offset + plot_height - mapLinearY(impedance[i].magnitude_ohm, min_mag, max_mag, plot_height);

        tft->drawLine((int16_t)x1, (int16_t)y1, (int16_t)x2, (int16_t)y2, TFT_GREEN);
    }

    // Draw axis labels
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextSize(1);

    // Y-axis labels (magnitude)
    for (uint8_t i = 0; i <= 4; i++) {
        float mag_value = min_mag + (max_mag - min_mag) * i / 4.0f;
        uint16_t y_pos = y_offset + plot_height - (i * plot_height / 4);
        tft->setCursor(5, y_pos - 4);
        tft->printf("%.0f", mag_value);
    }
}

void BodePlot::plotPhase(const std::vector<ImpedancePoint>& impedance, uint16_t y_offset) {
    uint32_t min_freq, max_freq;
    float min_phase, max_phase;

    findFrequencyRange(impedance, min_freq, max_freq);
    findPhaseRange(impedance, min_phase, max_phase);

    // Add 10% margin to phase range
    float phase_range = max_phase - min_phase;
    min_phase -= phase_range * 0.1f;
    max_phase += phase_range * 0.1f;

    // Draw axes and grid
    drawAxes(PLOT_MARGIN_LEFT, y_offset, plot_width, plot_height, "Phase (deg)", "");
    drawGrid(PLOT_MARGIN_LEFT, y_offset, plot_width, plot_height, 5, 4);

    // Plot data points
    for (size_t i = 1; i < impedance.size(); i++) {
        if (!impedance[i - 1].valid || !impedance[i].valid) continue;

        float x1 = PLOT_MARGIN_LEFT + mapLogX(impedance[i - 1].freq_hz, min_freq, max_freq, plot_width);
        float y1 = y_offset + plot_height - mapLinearY(impedance[i - 1].phase_deg, min_phase, max_phase, plot_height);

        float x2 = PLOT_MARGIN_LEFT + mapLogX(impedance[i].freq_hz, min_freq, max_freq, plot_width);
        float y2 = y_offset + plot_height - mapLinearY(impedance[i].phase_deg, min_phase, max_phase, plot_height);

        tft->drawLine((int16_t)x1, (int16_t)y1, (int16_t)x2, (int16_t)y2, TFT_CYAN);
    }

    // Draw axis labels
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextSize(1);

    // Y-axis labels (phase)
    for (uint8_t i = 0; i <= 4; i++) {
        float phase_value = min_phase + (max_phase - min_phase) * i / 4.0f;
        uint16_t y_pos = y_offset + plot_height - (i * plot_height / 4);
        tft->setCursor(5, y_pos - 4);
        tft->printf("%.0f", phase_value);
    }

    // X-axis labels (frequency) - only on bottom plot
    for (uint8_t i = 0; i <= 5; i++) {
        float log_min = log10f((float)min_freq);
        float log_max = log10f((float)max_freq);
        float log_freq = log_min + (log_max - log_min) * i / 5.0f;
        uint32_t freq_value = (uint32_t)powf(10.0f, log_freq);

        uint16_t x_pos = PLOT_MARGIN_LEFT + (i * plot_width / 5);
        tft->setCursor(x_pos - 15, y_offset + plot_height + 5);

        if (freq_value < 1000) {
            tft->printf("%lu", freq_value);
        } else if (freq_value < 1000000) {
            tft->printf("%.1fk", freq_value / 1000.0f);
        } else {
            tft->printf("%.1fM", freq_value / 1000000.0f);
        }
    }
}

void BodePlot::drawAxes(uint16_t x_start, uint16_t y_start, uint16_t width, uint16_t height,
                        const char* title, const char* y_label) {
    // Draw axes
    tft->drawLine(x_start, y_start, x_start, y_start + height, TFT_WHITE);                   // Y-axis
    tft->drawLine(x_start, y_start + height, x_start + width, y_start + height, TFT_WHITE);  // X-axis

    // Draw title
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextSize(1);
    tft->setCursor(x_start + 5, y_start - 15);
    tft->print(title);
}

void BodePlot::drawGrid(uint16_t x_start, uint16_t y_start, uint16_t width, uint16_t height,
                        uint8_t x_divisions, uint8_t y_divisions) {
    // Draw vertical grid lines
    for (uint8_t i = 1; i < x_divisions; i++) {
        uint16_t x = x_start + (i * width / x_divisions);
        for (uint16_t y = y_start; y < y_start + height; y += 4) {
            tft->drawPixel(x, y, TFT_DARKGREY);
        }
    }

    // Draw horizontal grid lines
    for (uint8_t i = 1; i < y_divisions; i++) {
        uint16_t y = y_start + (i * height / y_divisions);
        for (uint16_t x = x_start; x < x_start + width; x += 4) {
            tft->drawPixel(x, y, TFT_DARKGREY);
        }
    }
}

float BodePlot::mapLogX(uint32_t freq_hz, uint32_t min_freq, uint32_t max_freq, uint16_t plot_width) {
    float log_freq = log10f((float)freq_hz);
    float log_min = log10f((float)min_freq);
    float log_max = log10f((float)max_freq);

    return (log_freq - log_min) / (log_max - log_min) * plot_width;
}

float BodePlot::mapLinearY(float value, float min_val, float max_val, uint16_t plot_height) {
    if (max_val == min_val) return plot_height / 2.0f;
    return (value - min_val) / (max_val - min_val) * plot_height;
}

void BodePlot::findFrequencyRange(const std::vector<ImpedancePoint>& impedance, uint32_t& min_freq, uint32_t& max_freq) {
    min_freq = UINT32_MAX;
    max_freq = 0;

    for (const auto& point : impedance) {
        if (!point.valid) continue;
        if (point.freq_hz < min_freq) min_freq = point.freq_hz;
        if (point.freq_hz > max_freq) max_freq = point.freq_hz;
    }

    // Ensure valid range
    if (min_freq == UINT32_MAX) min_freq = 1;
    if (max_freq == 0) max_freq = 100000;
    if (min_freq == max_freq) max_freq = min_freq + 1;
}

void BodePlot::findMagnitudeRange(const std::vector<ImpedancePoint>& impedance, float& min_mag, float& max_mag) {
    min_mag = FLT_MAX;
    max_mag = -FLT_MAX;

    for (const auto& point : impedance) {
        if (!point.valid) continue;
        if (point.magnitude_ohm < min_mag) min_mag = point.magnitude_ohm;
        if (point.magnitude_ohm > max_mag) max_mag = point.magnitude_ohm;
    }

    // Ensure valid range
    if (min_mag == FLT_MAX) min_mag = 0.0f;
    if (max_mag == -FLT_MAX) max_mag = 1000.0f;
    if (min_mag == max_mag) {
        min_mag -= 1.0f;
        max_mag += 1.0f;
    }
}

void BodePlot::findPhaseRange(const std::vector<ImpedancePoint>& impedance, float& min_phase, float& max_phase) {
    min_phase = FLT_MAX;
    max_phase = -FLT_MAX;

    for (const auto& point : impedance) {
        if (!point.valid) continue;
        if (point.phase_deg < min_phase) min_phase = point.phase_deg;
        if (point.phase_deg > max_phase) max_phase = point.phase_deg;
    }

    // Ensure valid range
    if (min_phase == FLT_MAX) min_phase = -90.0f;
    if (max_phase == -FLT_MAX) max_phase = 90.0f;
    if (min_phase == max_phase) {
        min_phase -= 10.0f;
        max_phase += 10.0f;
    }
}
