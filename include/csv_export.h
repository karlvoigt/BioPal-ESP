#ifndef CSV_EXPORT_H
#define CSV_EXPORT_H

#include <Arduino.h>

/*=========================CSV EXPORT=========================*/
// Print all impedance data to Serial in CSV format for Excel/plotting
// Format: DUT,Frequency_Hz,Magnitude_Ohms,Phase_Deg
void printCSVToSerial();

#endif // CSV_EXPORT_H
