#include "calibration.h"
#include <LittleFS.h>

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
CalibrationPoint* getCalibrationPoint(uint32_t freq, bool highTIA, uint8_t pgaGain) {
    if(pgaGain > 7) return nullptr;

    int idx = findFrequencyIndex(freq);
    if(idx < 0) return nullptr;

    if(highTIA) {
        return &calibrationData[idx].high_TIA_gains[pgaGain];
    } else {
        return &calibrationData[idx].low_TIA_gains[pgaGain];
    }
}

/*=========================FILE LOADING FUNCTIONS=========================*/

// Load calibration data from filesystem
// CSV Format: freq,tia_mode,pga_gain,v_gain,i_gain,phase
// tia_mode: 0=low, 1=high
// pga_gain: 0-7
bool loadCalibrationData() {
    // Initialize LittleFS
    if(!LittleFS.begin(true)) {
        Serial.println("Failed to mount LittleFS");
        return false;
    }

    // Open calibration file
    File file = LittleFS.open("/calibration.csv", "r");
    if(!file) {
        Serial.println("Failed to open calibration.csv");
        LittleFS.end();
        return false;
    }

    numCalibrationFreqs = 0;
    int currentFreqIdx = -1;
    uint32_t lastFreq = 0;

    // Read file line by line
    while(file.available() && numCalibrationFreqs < MAX_CAL_FREQUENCIES) {
        String line = file.readStringUntil('\n');
        line.trim();

        // Skip empty lines and comments
        if(line.length() == 0 || line.startsWith("#")) {
            continue;
        }

        // Parse CSV line: freq,tia_mode,pga_gain,v_gain,i_gain,phase
        uint32_t freq = 0;
        int tia_mode = 0;
        int pga_gain = 0;
        float v_gain = 1.0;
        float i_gain = 1.0;
        float phase = 0.0;

        int fieldCount = sscanf(line.c_str(), "%lu,%d,%d,%f,%f,%f",
                                &freq, &tia_mode, &pga_gain, &v_gain, &i_gain, &phase);

        if(fieldCount != 6) {
            Serial.printf("Invalid line: %s\n", line.c_str());
            continue;
        }

        // Validate ranges
        if(tia_mode < 0 || tia_mode > 1 || pga_gain < 0 || pga_gain > 7) {
            Serial.printf("Invalid TIA mode or PGA gain: %s\n", line.c_str());
            continue;
        }

        // Check if this is a new frequency
        if(freq != lastFreq) {
            currentFreqIdx = findFrequencyIndex(freq);
            if(currentFreqIdx < 0) {
                // New frequency, add it
                currentFreqIdx = numCalibrationFreqs;
                calibrationData[currentFreqIdx].frequency_hz = freq;
                numCalibrationFreqs++;
                lastFreq = freq;
            }
        }

        // Store calibration point
        CalibrationPoint point(v_gain, i_gain, phase);
        if(tia_mode == 0) {
            calibrationData[currentFreqIdx].low_TIA_gains[pga_gain] = point;
        } else {
            calibrationData[currentFreqIdx].high_TIA_gains[pga_gain] = point;
        }
    }

    file.close();
    LittleFS.end();

    Serial.printf("Loaded calibration data for %d frequencies\n", numCalibrationFreqs);
    return true;
}

bool calibrate(MeasurementPoint& point) {
    CalibrationPoint* calPoint = getCalibrationPoint(point.freq_hz, point.tia_gain, point.pga_gain);
    if(calPoint) {
        // Apply calibration
        point.V_magnitude = point.V_magnitude / 64.0 * (2.0*3.3)/4096.0 / calPoint->voltage_gain;
        point.I_magnitude = point.I_magnitude / 64.0 * (2.0*3.3)/4096.0 / calPoint->current_gain;
        point.phase_deg -= calPoint->phase_offset;
        return true;
    } else {
        return false; // Calibration point not found
    }
}