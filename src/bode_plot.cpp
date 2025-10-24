#include "bode_plot.h"
#include "defines.h"
#include <TFT_eSPI.h>
#include <math.h>

// TFT instance (shared with gui_screens.cpp)
TFT_eSPI tft = TFT_eSPI();

// Screen dimensions (landscape orientation)
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// Plot area margins
#define MARGIN_LEFT   50
#define MARGIN_RIGHT  50
#define MARGIN_TOP    30
#define MARGIN_BOTTOM 40

// Plot area dimensions
#define PLOT_WIDTH  (SCREEN_WIDTH - MARGIN_LEFT - MARGIN_RIGHT)
#define PLOT_HEIGHT (SCREEN_HEIGHT - MARGIN_TOP - MARGIN_BOTTOM)

// Plot origin (bottom-left corner)
#define PLOT_X0 MARGIN_LEFT
#define PLOT_Y0 (SCREEN_HEIGHT - MARGIN_BOTTOM)

// Colors
#define COLOR_BG        TFT_BLACK
#define COLOR_GRID      TFT_DARKGREY
#define COLOR_AXIS      TFT_WHITE
#define COLOR_MAG       TFT_CYAN      // Magnitude line
#define COLOR_PHASE     TFT_YELLOW    // Phase line
#define COLOR_TEXT      TFT_WHITE

/*=========================HELPER FUNCTIONS=========================*/

// Map frequency to X pixel coordinate (log scale)
static int16_t freqToX(float freq_hz, float freq_min, float freq_max) {
    if (freq_hz <= 0 || freq_min <= 0 || freq_max <= 0) return PLOT_X0;

    float log_freq = log10f(freq_hz);
    float log_min = log10f(freq_min);
    float log_max = log10f(freq_max);

    float normalized = (log_freq - log_min) / (log_max - log_min);
    return PLOT_X0 + (int16_t)(normalized * PLOT_WIDTH);
}

// Map magnitude to Y pixel coordinate (log scale, inverted for screen)
static int16_t magToY(float mag, float mag_min, float mag_max) {
    if (mag <= 0 || mag_min <= 0 || mag_max <= 0) return PLOT_Y0;

    float log_mag = log10f(mag);
    float log_min = log10f(mag_min);
    float log_max = log10f(mag_max);

    float normalized = (log_mag - log_min) / (log_max - log_min);
    // Invert Y because screen coordinates go top-to-bottom
    return PLOT_Y0 - (int16_t)(normalized * PLOT_HEIGHT);
}

// Map phase to Y pixel coordinate (linear scale, inverted for screen)
static int16_t phaseToY(float phase_deg, float phase_min, float phase_max) {
    float normalized = (phase_deg - phase_min) / (phase_max - phase_min);
    // Invert Y because screen coordinates go top-to-bottom
    return PLOT_Y0 - (int16_t)(normalized * PLOT_HEIGHT);
}

// Draw dashed line
static void drawDashedLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;

    int16_t dashCount = 0;
    const int16_t dashLength = 5;
    const int16_t gapLength = 3;
    bool drawing = true;

    while (true) {
        if (drawing) {
            tft.drawPixel(x0, y0, color);
        }

        dashCount++;
        if (dashCount >= (drawing ? dashLength : gapLength)) {
            dashCount = 0;
            drawing = !drawing;
        }

        if (x0 == x1 && y0 == y1) break;

        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/*=========================PUBLIC FUNCTIONS=========================*/

void initBodePlot() {
    tft.init();
    tft.setRotation(1);  // Landscape orientation (0=portrait, 1=landscape)
    tft.fillScreen(COLOR_BG);

    Serial.println("TFT initialized for Bode plots (landscape mode)");
}

void drawBodePlot(uint8_t dutIndex) {
    if (dutIndex >= MAX_DUT_COUNT) {
        Serial.printf("ERROR: Invalid DUT index %d\n", dutIndex);
        return;
    }

    int numPoints = frequencyCount[dutIndex];
    if (numPoints == 0) {
        Serial.printf("WARNING: No data for DUT %d\n", dutIndex + 1);
        return;
    }

    Serial.printf("Drawing Bode plot for DUT %d (%d points)\n", dutIndex + 1, numPoints);

    // Find data ranges
    float freq_min = 1e9, freq_max = 0;
    float mag_min = 1e9, mag_max = 0;
    float phase_min = 1e9, phase_max = -1e9;

    for (int i = 0; i < numPoints; i++) {
        ImpedancePoint* point = &baselineImpedanceData[dutIndex][i];
        if (!point->valid) continue;

        if (point->freq_hz > 0) {
            if (point->freq_hz < freq_min) freq_min = point->freq_hz;
            if (point->freq_hz > freq_max) freq_max = point->freq_hz;
        }

        if (point->Z_magnitude > 0) {
            if (point->Z_magnitude < mag_min) mag_min = point->Z_magnitude;
            if (point->Z_magnitude > mag_max) mag_max = point->Z_magnitude;
        }

        if (point->Z_phase < phase_min) phase_min = point->Z_phase;
        if (point->Z_phase > phase_max) phase_max = point->Z_phase;
    }

    // Round to nice power-of-10 boundaries for log scales
    int freq_min_exp = (int)floorf(log10f(freq_min));
    int freq_max_exp = (int)ceilf(log10f(freq_max));
    freq_min = powf(10.0f, freq_min_exp);
    freq_max = powf(10.0f, freq_max_exp);

    int mag_min_exp = (int)floorf(log10f(mag_min));
    int mag_max_exp = (int)ceilf(log10f(mag_max));
    mag_min = powf(10.0f, mag_min_exp);
    mag_max = powf(10.0f, mag_max_exp);

    // Add padding to phase range
    float phase_range = phase_max - phase_min;
    phase_min -= phase_range * 0.05f;
    phase_max += phase_range * 0.05f;

    // Clear screen
    tft.fillScreen(COLOR_BG);

    // Draw title
    tft.setTextColor(COLOR_TEXT);
    tft.setTextSize(2);
    tft.setCursor(10, 5);
    tft.printf("DUT %d Bode Plot", dutIndex + 1);

    // Draw axes
    tft.drawLine(PLOT_X0, PLOT_Y0, PLOT_X0 + PLOT_WIDTH, PLOT_Y0, COLOR_AXIS);  // X-axis
    tft.drawLine(PLOT_X0, PLOT_Y0, PLOT_X0, PLOT_Y0 - PLOT_HEIGHT, COLOR_AXIS); // Y-axis

    // Draw grid lines at powers of 10 for frequency (X-axis)
    for (int exp = freq_min_exp; exp <= freq_max_exp; exp++) {
        float freq = powf(10.0f, exp);
        int16_t x = freqToX(freq, freq_min, freq_max);
        if (x > PLOT_X0 && x < PLOT_X0 + PLOT_WIDTH) {
            tft.drawLine(x, PLOT_Y0, x, PLOT_Y0 - PLOT_HEIGHT, COLOR_GRID);
        }
    }

    // Draw grid lines at powers of 10 for magnitude (Y-axis)
    for (int exp = mag_min_exp; exp <= mag_max_exp; exp++) {
        float mag = powf(10.0f, exp);
        int16_t y = magToY(mag, mag_min, mag_max);
        if (y > PLOT_Y0 - PLOT_HEIGHT && y < PLOT_Y0) {
            tft.drawLine(PLOT_X0, y, PLOT_X0 + PLOT_WIDTH, y, COLOR_GRID);
        }
    }

    // Axis labels
    tft.setTextSize(1);

    // X-axis label (frequency)
    tft.setCursor(SCREEN_WIDTH / 2 - 30, SCREEN_HEIGHT - 10);
    tft.print("Frequency (Hz)");

    // Y-axis labels (magnitude left, phase right)
    tft.setTextColor(COLOR_MAG);
    tft.setCursor(5, SCREEN_HEIGHT / 2);
    tft.print("|Z|");

    tft.setTextColor(COLOR_PHASE);
    tft.setCursor(SCREEN_WIDTH - 40, SCREEN_HEIGHT / 2);
    tft.print("Phase");

    // Tick labels in scientific notation
    tft.setTextSize(1);
    char buf[16];

    // Frequency ticks (X-axis) - scientific notation
    tft.setTextColor(COLOR_TEXT);
    for (int exp = freq_min_exp; exp <= freq_max_exp; exp++) {
        float freq = powf(10.0f, exp);
        int16_t x = freqToX(freq, freq_min, freq_max);

        if (x >= PLOT_X0 && x <= PLOT_X0 + PLOT_WIDTH) {
            snprintf(buf, sizeof(buf), "10^%d", exp);
            tft.setCursor(x - 15, PLOT_Y0 + 5);
            tft.print(buf);
        }
    }

    // Magnitude ticks (Y-axis left) - scientific notation
    tft.setTextColor(COLOR_MAG);
    for (int exp = mag_min_exp; exp <= mag_max_exp; exp++) {
        float mag = powf(10.0f, exp);
        int16_t y = magToY(mag, mag_min, mag_max);

        if (y >= PLOT_Y0 - PLOT_HEIGHT && y <= PLOT_Y0) {
            snprintf(buf, sizeof(buf), "10^%d", exp);
            tft.setCursor(2, y - 4);
            tft.print(buf);
        }
    }

    // Phase ticks (Y-axis right) - degrees
    tft.setTextColor(COLOR_PHASE);
    int phase_step = (int)((phase_max - phase_min) / 4);
    if (phase_step < 10) phase_step = 10;

    for (int phase_val = (int)phase_min; phase_val <= (int)phase_max; phase_val += phase_step) {
        int16_t y = phaseToY(phase_val, phase_min, phase_max);

        if (y >= PLOT_Y0 - PLOT_HEIGHT && y <= PLOT_Y0) {
            snprintf(buf, sizeof(buf), "%d", phase_val);
            tft.setCursor(SCREEN_WIDTH - 25, y - 4);
            tft.print(buf);
        }
    }

    // Plot magnitude data (solid line)
    int16_t prevX_mag = -1, prevY_mag = -1;
    for (int i = 0; i < numPoints; i++) {
        ImpedancePoint* point = &baselineImpedanceData[dutIndex][i];
        if (!point->valid || point->freq_hz <= 0 || point->Z_magnitude <= 0) continue;

        int16_t x = freqToX(point->freq_hz, freq_min, freq_max);
        int16_t y = magToY(point->Z_magnitude, mag_min, mag_max);

        if (prevX_mag >= 0 && prevY_mag >= 0) {
            tft.drawLine(prevX_mag, prevY_mag, x, y, COLOR_MAG);
        }

        prevX_mag = x;
        prevY_mag = y;
    }

    // Plot phase data (dashed line)
    int16_t prevX_phase = -1, prevY_phase = -1;
    for (int i = 0; i < numPoints; i++) {
        ImpedancePoint* point = &baselineImpedanceData[dutIndex][i];
        if (!point->valid || point->freq_hz <= 0) continue;

        int16_t x = freqToX(point->freq_hz, freq_min, freq_max);
        int16_t y = phaseToY(point->Z_phase, phase_min, phase_max);

        if (prevX_phase >= 0 && prevY_phase >= 0) {
            drawDashedLine(prevX_phase, prevY_phase, x, y, COLOR_PHASE);
        }

        prevX_phase = x;
        prevY_phase = y;
    }

    Serial.printf("Bode plot drawn for DUT %d\n", dutIndex + 1);
}
