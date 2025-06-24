#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#define CHANNEL_MAX 13
#define NUM_FRAMES_PER_DEAUTH 10
#define SERIAL_DEBUG
#ifdef SERIAL_DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(x, ...) Serial.printf(x, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(x, ...)
#endif

#endif // DEFINITIONS_H