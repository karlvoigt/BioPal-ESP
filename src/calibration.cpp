#include "calibration.h"
#include <LittleFS.h>

// float v_phase_shifts[MAX_CAL_FREQUENCIES] = {
//     0.0f, // 1 Hz
//     0.0f, // 2 Hz
//     0.0f, // 4 Hz
//     0.0f, // 5 Hz
//     0.0f, // 8 Hz
//     0.0f, // 10 Hz
//     0.0f, // 16 Hz
//     0.0f, // 20 Hz
//     0.0f, // 25 Hz
//     0.0f, // 32 Hz
//     0.0f, // 40 Hz
//     -0.01f, // 50 Hz
//     -0.01f, // 80 Hz
//     -0.01f, // 100 Hz
//     -0.01f, // 125 Hz
//     -0.02f, // 160 Hz
//     -0.02f, // 200 Hz
//     -0.03f, // 250 Hz
//     -0.04f, // 400 Hz
//     -0.05f, // 500 Hz
//     -0.07f, // 625 Hz
//     -0.09f, // 800 Hz
//     -0.11f, // 1000 Hz
//     -0.17f, // 1250 Hz
//     -0.21f, // 2000 Hz
//     -0.27f, // 2500 Hz
//     -0.34f, // 3125 Hz
//     -0.43f, // 4000 Hz
//     -0.54f, // 5000 Hz
//     -0.86f, // 6250 Hz
//     -1.07f, // 10000 Hz
//     -1.34f, // 12500 Hz
//     -2.15f, // 15625 Hz
//     -4.29f, // 25000 Hz
//     -5.36f, // 50000 Hz
//     -6.84f, // 62500 Hz
//     -8.53f  // 100000 Hz
// };

// float v_gain[MAX_CAL_FREQUENCIES] = {
//     300.0f, // 1 Hz
//     300.0f, // 2 Hz
//     300.0f, // 4 Hz
//     300.0f, // 5 Hz
//     300.0f, // 8 Hz
//     300.0f, // 10 Hz
//     300.0f, // 16 Hz
//     300.0f, // 20 Hz
//     300.0f, // 25 Hz
//     300.0f, // 32 Hz
//     300.0f, // 40 Hz
//     300.0f, // 50 Hz
//     300.0f, // 80 Hz
//     300.0f, // 100 Hz
//     300.0f, // 125 Hz
//     300.0f, // 160 Hz
//     300.0f, // 200 Hz
//     300.0f, // 250 Hz
//     300.0f, // 400 Hz
//     300.0f, // 500 Hz
//     300.0f, // 625 Hz
//     300.0f, // 800 Hz
//     300.0f, // 1000 Hz
//     300.0f, // 1250 Hz
//     300.0f, // 2000 Hz
//     300.0f, // 2500 Hz
//     300.0f, // 3125 Hz
//     300.0f, // 4000 Hz
//     300.0f, // 5000 Hz
//     300.0f, // 6250 Hz
//     300.0f, // 10000 Hz
//     300.0f, // 12500 Hz
//     300.0f, // 15625 Hz
//     300.0f, // 25000 Hz
//     300.0f, // 50000 Hz
//     300.0f, // 62500 Hz
//     300.0f  // 100000 Hz
// };

// float I_low_phase_shift[MAX_CAL_FREQUENCIES] = {
//     0.00f,  // 1 Hz
//     0.00f,  // 2 Hz
//     0.00f,  // 4 Hz
//     0.00f,  // 5 Hz
//     0.00f,  // 8 Hz
//     0.00f,  // 10 Hz
//     0.00f,  // 16 Hz
//     0.00f,  // 20 Hz
//     0.00f,  // 25 Hz
//     0.00f,  // 32 Hz
//     0.00f,  // 40 Hz
//     0.00f,  // 50 Hz
//     0.00f,  // 80 Hz
//     0.00f,  // 100 Hz
//     -0.01f, // 125 Hz
//     -0.01f, // 160 Hz
//     -0.01f, // 200 Hz
//     -0.01f, // 250 Hz
//     -0.01f, // 400 Hz
//     -0.02f, // 500 Hz
//     -0.03f, // 625 Hz
//     -0.03f, // 800 Hz
//     -0.04f, // 1000 Hz
//     -0.05f, // 1250 Hz
//     -0.07f, // 2000 Hz
//     -0.11f, // 2500 Hz
//     -0.13f, // 3125 Hz
//     -0.17f, // 4000 Hz
//     -0.21f, // 5000 Hz
//     -0.27f, // 6250 Hz
//     -0.34f, // 10000 Hz
//     -0.54f, // 12500 Hz
//     -0.67f, // 15625 Hz
//     -0.84f, // 25000 Hz
//     -1.34f, // 50000 Hz
//     -2.68f, // 62500 Hz
//     -3.35f, // 100000 Hz
//     -4.29f, // 125000 Hz
//     -5.36f  // 156250 Hz
// };

// float I_low_gain[MAX_CAL_FREQUENCIES] = {
//     37.50f, // 1 Hz
//     37.50f, // 2 Hz
//     37.50f, // 4 Hz
//     37.50f, // 5 Hz
//     37.50f, // 8 Hz
//     37.50f, // 10 Hz
//     37.50f, // 16 Hz
//     37.50f, // 20 Hz
//     37.50f, // 25 Hz
//     37.50f, // 32 Hz
//     37.50f, // 40 Hz
//     37.50f, // 50 Hz
//     37.50f, // 80 Hz
//     37.50f, // 100 Hz
//     37.50f, // 125 Hz
//     37.50f, // 160 Hz
//     37.50f, // 200 Hz
//     37.50f, // 250 Hz
//     37.50f, // 400 Hz
//     37.50f, // 500 Hz
//     37.50f, // 625 Hz
//     37.50f, // 800 Hz
//     37.50f, // 1000 Hz
//     37.50f, // 1250 Hz
//     37.50f, // 2000 Hz
//     37.50f, // 2500 Hz
//     37.50f, // 3125 Hz
//     37.50f, // 4000 Hz
//     37.50f, // 5000 Hz
//     37.50f, // 6250 Hz
//     37.50f, // 10000 Hz
//     37.50f, // 12500 Hz
//     37.50f, // 15625 Hz
//     37.50f, // 25000 Hz
//     37.49f, // 50000 Hz
//     37.46f, // 62500 Hz
//     37.44f, // 100000 Hz
//     37.39f, // 125000 Hz
//     37.34f  // 156250 Hz
// };

// float I_high_phase_shift[MAX_CAL_FREQUENCIES] = {
//     -0.01f,  // 1 Hz
//     -0.02f,  // 2 Hz
//     -0.04f,  // 4 Hz
//     -0.05f,  // 5 Hz
//     -0.09f,  // 8 Hz
//     -0.11f,  // 10 Hz
//     -0.17f,  // 16 Hz
//     -0.21f,  // 20 Hz
//     -0.27f,  // 25 Hz
//     -0.34f,  // 32 Hz
//     -0.43f,  // 40 Hz
//     -0.54f,  // 50 Hz
//     -0.86f,  // 80 Hz
//     -1.07f,  // 100 Hz
//     -1.34f,  // 125 Hz
//     -1.72f,  // 160 Hz
//     -2.15f,  // 200 Hz
//     -2.68f,  // 250 Hz
//     -4.29f,  // 400 Hz
//     -5.36f,  // 500 Hz
//     -6.68f,  // 625 Hz
//     -8.53f,  // 800 Hz
//     -10.62f, // 1000 Hz
//     -13.19f, // 1250 Hz
//     -20.56f, // 2000 Hz
//     -25.11f, // 2500 Hz
//     -30.37f, // 3125 Hz
//     -36.87f, // 4000 Hz
//     -43.15f, // 5000 Hz
//     -49.52f, // 6250 Hz
//     -61.93f, // 10000 Hz
//     -66.89f, // 12500 Hz
//     -71.15f, // 15625 Hz
//     -77.96f, // 25000 Hz
//     -83.91f, // 50000 Hz
//     -85.12f, // 62500 Hz
//     -86.19f, // 100000 Hz
//     -86.95f, // 125000 Hz
//     -87.49f  // 156250 Hz
// };

// float I_high_gain[MAX_CAL_FREQUENCIES] = {
//     7500.0f, // 1 Hz
//     7500.0f, // 2 Hz
//     7500.0f, // 4 Hz
//     7500.0f, // 5 Hz
//     7500.0f, // 8 Hz
//     7500.0f, // 10 Hz
//     7500.0f, // 16 Hz
//     7500.0f, // 20 Hz
//     7500.0f, // 25 Hz
//     7500.0f, // 32 Hz
//     7500.0f, // 40 Hz
//     7500.0f, // 50 Hz
//     7499.0f, // 80 Hz
//     7499.0f, // 100 Hz
//     7498.0f, // 125 Hz
//     7497.0f, // 160 Hz
//     7495.0f, // 200 Hz
//     7492.0f, // 250 Hz
//     7479.0f, // 400 Hz
//     7467.0f, // 500 Hz
//     7449.0f, // 625 Hz
//     7417.0f, // 800 Hz
//     7372.0f, // 1000 Hz
//     7302.0f, // 1250 Hz
//     7022.0f, // 2000 Hz
//     6791.0f, // 2500 Hz
//     6471.0f, // 3125 Hz
//     6000.0f, // 4000 Hz
//     5472.0f, // 5000 Hz
//     4868.0f, // 6250 Hz
//     3529.0f, // 10000 Hz
//     2943.0f, // 12500 Hz
//     2423.0f, // 15625 Hz
//     1565.0f, // 25000 Hz
//     795.0f,  // 50000 Hz
//     638.0f,  // 62500 Hz
//     499.0f,  // 100000 Hz
//     399.0f,  // 125000 Hz
//     319.0f   // 156250 Hz
// };

const float V_GBW = 10.0f;  // Gain Bandwidth Product in MHz
const float V_gain = 15.4f; // Voltage Gain of the INA331 Instrumentation Amplifier
const float PGA_Cutoff[8] = { // Cutoff frequencies for each PGA gain setting (in MHz)
    10.0f,  // Gain = 1
    3.8f,   // Gain = 2
    1.8f,   // Gain = 5
    1.8f,   // Gain = 10
    1.3f,   // Gain = 20
    0.9f,   // Gain = 50
    0.38f,  // Gain = 100
    0.23f   // Gain = 200
};
const float I_GBW = 40.0f;  // Gain Bandwidth Product in MHz
const float TIA_Gains[2] = {7500.0f, 37.5f}; // TIA Gains for High and Low modes respectively
const float TLV_gain = 20.0f; // Gain of the TLV9061 OpAmp which is identical on both current and voltage stages

// const float non_ideal_phase_shift[38] = {

// }
/**
 * @brief PGA113 Gain enumeration (Scope gains)
 */
typedef enum {
    PGA113_GAIN_1 = 0,    /**< Gain = 1 (0000) */
    PGA113_GAIN_2 = 1,    /**< Gain = 2 (0001) */
    PGA113_GAIN_5 = 2,    /**< Gain = 5 (0010) */
    PGA113_GAIN_10 = 3,   /**< Gain = 10 (0011) */
    PGA113_GAIN_20 = 4,   /**< Gain = 20 (0100) */
    PGA113_GAIN_50 = 5,   /**< Gain = 50 (0101) */
    PGA113_GAIN_100 = 6,  /**< Gain = 100 (0110) */
    PGA113_GAIN_200 = 7   /**< Gain = 200 (0111) */
} PGA113_Gain_t;

#define PGA113_ENUM_TO_GAIN(gain_enum)  \
    ((gain_enum) == PGA113_GAIN_1 ? 1 : \
    (gain_enum) == PGA113_GAIN_2 ? 2 : \
    (gain_enum) == PGA113_GAIN_5 ? 5 : \
    (gain_enum) == PGA113_GAIN_10 ? 10 : \
    (gain_enum) == PGA113_GAIN_20 ? 20 : \
    (gain_enum) == PGA113_GAIN_50 ? 50 : \
    (gain_enum) == PGA113_GAIN_100 ? 100 : \
    (gain_enum) == PGA113_GAIN_200 ? 200 : 1)  // Default to 1 if invalid
/* Exported functions prototypes ---------------------------------------------*/

/*=========================GLOBAL VARIABLES=========================*/
FreqCalibrationData calibrationData[MAX_CAL_FREQUENCIES];
int numCalibrationFreqs = 0;

/*=========================HELPER FUNCTIONS=========================*/

// Find the index of a frequency in the calibration data
int findFrequencyIndex(uint32_t freq) {
    for(int i = 0; i < numCalibrationFreqs; i++) {
        if(calibrationData[i].frequency_hz == freq) {
            return i;
        }
    }
    return -1;
}

// Get calibration point for specific frequency and gain settings
CalibrationPoint* getCalibrationPoint(uint32_t freq, bool lowTIA, uint8_t pgaGain) {
    if(pgaGain > 7) return nullptr;

    // Calculate our voltage gain and phase shift
    float v_gain = 20 * V_gain * (1/sqrt(1+(pow((freq/(V_GBW/V_gain*1e6)), 2))));
    float v_phase = -atan(freq/(V_GBW/V_gain*1e6)) * 180.0f / M_PI;

    // Calculate our current gain and phase shift
    float tia_gain = TIA_Gains[lowTIA];
    float i_gain = 20 * tia_gain * (1/sqrt(1+(pow((freq/(I_GBW/tia_gain*1e6)), 2)))) 
                   * PGA113_ENUM_TO_GAIN(pgaGain) * (1/sqrt(1+(pow((freq/(PGA_Cutoff[pgaGain]*1e6)), 2))));
    float i_phase = -atan(freq/(I_GBW/tia_gain*1e6)) * 180.0f / M_PI - atan(freq/(PGA_Cutoff[pgaGain]*1e6)) * 180.0f / M_PI;

    return new CalibrationPoint(v_gain, i_gain, v_phase - i_phase);
    // int idx = findFrequencyIndex(freq);
    // if(idx < 0) return nullptr;

    // if(highTIA) {
    //     return &calibrationData[idx].high_TIA_gains[pgaGain];
    // } else {
    //     return &calibrationData[idx].low_TIA_gains[pgaGain];
    // }
}

/*=========================FILE LOADING FUNCTIONS=========================*/

// Load calibration data from filesystem
// CSV Format: freq,tia_mode,pga_gain,v_gain,i_gain,phase
// tia_mode: 0=low, 1=high
// pga_gain: 0-7
bool loadCalibrationData() {
    // // Initialize LittleFS
    // if(!LittleFS.begin(true)) {
    //     Serial.println("Failed to mount LittleFS");
    //     return false;
    // }

    // // Open calibration file
    // File file = LittleFS.open("/calibration.csv", "r");
    // if(!file) {
    //     Serial.println("Failed to open calibration.csv");
    //     LittleFS.end();
    //     return false;
    // }

    // numCalibrationFreqs = 0;
    // int currentFreqIdx = -1;
    // uint32_t lastFreq = 0;

    // // Read file line by line
    // while(file.available() && numCalibrationFreqs < MAX_CAL_FREQUENCIES) {
    //     String line = file.readStringUntil('\n');
    //     line.trim();

    //     // Skip empty lines and comments
    //     if(line.length() == 0 || line.startsWith("#")) {
    //         continue;
    //     }

    //     // Parse CSV line: freq,tia_mode,pga_gain,v_gain,i_gain,phase
    //     uint32_t freq = 0;
    //     int tia_mode = 0;
    //     int pga_gain = 0;
    //     float v_gain = 1.0;
    //     float i_gain = 1.0;
    //     float phase = 0.0;

    //     int fieldCount = sscanf(line.c_str(), "%lu,%d,%d,%f,%f,%f",
    //                             &freq, &tia_mode, &pga_gain, &v_gain, &i_gain, &phase);

    //     if(fieldCount != 6) {
    //         Serial.printf("Invalid line: %s\n", line.c_str());
    //         continue;
    //     }

    //     // Validate ranges
    //     if(tia_mode < 0 || tia_mode > 1 || pga_gain < 0 || pga_gain > 7) {
    //         Serial.printf("Invalid TIA mode or PGA gain: %s\n", line.c_str());
    //         continue;
    //     }

    //     // Check if this is a new frequency
    //     if(freq != lastFreq) {
    //         currentFreqIdx = findFrequencyIndex(freq);
    //         if(currentFreqIdx < 0) {
    //             // New frequency, add it
    //             currentFreqIdx = numCalibrationFreqs;
    //             calibrationData[currentFreqIdx].frequency_hz = freq;
    //             numCalibrationFreqs++;
    //             lastFreq = freq;
    //         }
    //     }

    //     // Store calibration point
    //     CalibrationPoint point(v_gain, i_gain, phase);
    //     if(tia_mode == 0) {
    //         calibrationData[currentFreqIdx].low_TIA_gains[pga_gain] = point;
    //     } else {
    //         calibrationData[currentFreqIdx].high_TIA_gains[pga_gain] = point;
    //     }
    // }

    // file.close();
    // LittleFS.end();

    // Serial.printf("Loaded calibration data for %d frequencies\n", numCalibrationFreqs);
    return true;
}

bool calibrate(MeasurementPoint& point) {
    CalibrationPoint* calPoint = getCalibrationPoint(point.freq_hz, point.tia_gain, point.pga_gain);
    if(calPoint) {
        // Apply calibration
        // point.V_magnitude = point.V_magnitude / 64.0 * (2.0*3.3)/4096.0 / calPoint->voltage_gain;
        point.V_magnitude = point.V_magnitude / calPoint->voltage_gain;
        point.I_magnitude = point.I_magnitude / calPoint->current_gain;
        point.phase_deg += calPoint->phase_offset;
        return true;
    } else {
        return false; // Calibration point not found
    }
}