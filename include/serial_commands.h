#ifndef SERIAL_COMMANDS_H
#define SERIAL_COMMANDS_H

#include <Arduino.h>

/*=========================SERIAL COMMANDS=========================*/
// Process serial commands from computer (USB Serial)
// Call this regularly from a task to check for commands
// Commands:
//   start [num_duts]  - Start measurement (default 4 DUTs, or specify 1-4)
//   stop              - Stop measurement
void processSerialCommands();

#endif // SERIAL_COMMANDS_H
