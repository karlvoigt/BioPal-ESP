#include "csv_export.h"
#include "defines.h"

void printCSVToSerial() {
    Serial.println("\n\n========== IMPEDANCE DATA CSV ==========");
    Serial.println("DUT,Frequency_Hz,Magnitude_Ohms,Phase_Deg");

    for (uint8_t dut = 0; dut < MAX_DUT_COUNT; dut++) {
        for (int freqIdx = 0; freqIdx < frequencyCount[dut]; freqIdx++) {
            ImpedancePoint* point = &impedanceData[dut][freqIdx];

            if (point->valid) {
                Serial.printf("%d,%lu,%.6f,%.2f\n",
                             dut + 1,  // DUT numbering 1-4
                             point->freq_hz,
                             point->Z_magnitude,
                             point->Z_phase);
            }
        }
    }

    Serial.println("========================================\n");
}
