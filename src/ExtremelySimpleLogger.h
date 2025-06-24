// ExtremelySimpleLogger.h
#ifndef EXTREMELY_SIMPLE_LOGGER_H
#define EXTREMELY_SIMPLE_LOGGER_H

#include <Arduino.h> 

// Define DEBUG, to turn on logging. Comment out this line to turn off logging
#define DEBUG

#ifdef DEBUG
#define LOG(s) Serial.print(s)
#define LOG_LN(s) Serial.println(s)
#else
#define LOG(s) ((void)0) // Empty operation (noop) so that the macro is not empty and doesn't trigger errors
#define LOG_LN(s) ((void)0)
#endif

#endif // EXTREMELY_SIMPLE_LOGGER_H