#ifndef MODUBOTIC_GY33_COLOR_SENSOR_H
#define MODUBOTIC_GY33_COLOR_SENSOR_H

#include <Arduino.h>

void setupGy33ColorSensor();
void updateGy33ColorSensor();
void printGy33ReadingToSerial();
void printGy33TelemetryToStream(Stream &stream);
void clearGy33ReadingUpdated();
void scanGy33I2cBus();
void debugGy33ColorSensor();
void requestGy33ColorReading();
void setGy33LedPower(byte power);
void enableGy33ColorSensor(bool enabled);
bool isGy33ColorSensorEnabled();
bool isGy33ColorSensorAvailable();
bool hasGy33Reading();
bool isGy33ReadingOk();
byte getGy33Red8();
byte getGy33Green8();
byte getGy33Blue8();

#endif
