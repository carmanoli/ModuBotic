#ifndef MODUBOTIC_LCD_DISPLAY_H
#define MODUBOTIC_LCD_DISPLAY_H

#include <Arduino.h>

void setupLcd();
void updateLcd(const String &lastCommand);
void updateLcdTelemetry(float temperatureC, float humidity, bool readingOk);
void enableLcd(bool enabled);
bool isLcdEnabled();
bool isLcdAvailable();
byte getLcdAddress();

#endif
