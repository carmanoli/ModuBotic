#ifndef MODUBOTIC_AHT20_BMP280_SENSOR_H
#define MODUBOTIC_AHT20_BMP280_SENSOR_H

#include <Arduino.h>

void setupAht20Bmp280Sensor();
void updateAht20Bmp280Sensor();
void printAht20Bmp280ReadingToSerial();
void printAht20Bmp280TelemetryToStream(Stream &stream);
void clearAht20Bmp280ReadingUpdated();
void scanAht20Bmp280I2cBus();
void requestAht20Bmp280Reading();
void enableAht20Bmp280Sensor(bool enabled);
bool isAht20Bmp280SensorEnabled();
bool isAht20Bmp280SensorAvailable();
bool hasAht20Bmp280Reading();
bool isAht20Bmp280ReadingOk();
float getAht20TemperatureC();
float getAht20Humidity();
float getBmp280TemperatureC();
float getBmp280PressureHpa();

#endif
