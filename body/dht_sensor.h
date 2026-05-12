#ifndef MODUBOTIC_DHT_SENSOR_H
#define MODUBOTIC_DHT_SENSOR_H

#include <Arduino.h>

void setupDhtSensor();
void updateDhtSensor();
void printDhtReadingToSerial();
void printDhtTelemetryToStream(Stream &stream);
void clearDhtReadingUpdated();
void selectDhtLeftArm();
void selectDhtRightArm();
void selectDhtDirectD7();
void selectDhtDirectD12();
void selectDhtDirectD13();
void disableDhtSensor();
bool isDhtUsingPin(int pin);
bool hasDhtReading();
bool isDhtReadingOk();
float getDhtTemperatureC();
float getDhtHumidity();

#endif
