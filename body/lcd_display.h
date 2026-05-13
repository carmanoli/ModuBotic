#ifndef MODUBOTIC_LCD_DISPLAY_H
#define MODUBOTIC_LCD_DISPLAY_H

#include <Arduino.h>

void setupLcd();
void updateLcd(const String &lastCommand);
void updateLcdTelemetry(float temperatureC, float humidity, bool readingOk);
void enableLcd(bool enabled);
void setLcdBacklightFull();
void setLcdBacklightDim();
void sleepLcdDisplay();
void showLcdMessage(const char *line1, const char *line2);
bool isLcdEnabled();
bool isLcdAvailable();
byte getLcdAddress();

#endif
