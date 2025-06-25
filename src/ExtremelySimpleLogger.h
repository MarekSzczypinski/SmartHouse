// ExtremelySimpleLogger.h
#ifndef EXTREMELY_SIMPLE_LOGGER_H
#define EXTREMELY_SIMPLE_LOGGER_H

#include <Arduino.h>

#ifdef DEBUG_MODE
#define LOG(s) Serial.print(s)
#define LOG_LN(s) Serial.println(s)
#define LOG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
#define LOG(s) ((void)0) // Empty operation (noop) so that the macro is not empty and doesn't trigger errors
#define LOG_LN(s) ((void)0)
#define LOG_PRINTF(fmt, ...) ((void)0)
#endif

#endif // EXTREMELY_SIMPLE_LOGGER_H