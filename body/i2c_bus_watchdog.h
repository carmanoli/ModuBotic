#ifndef MODUBOTIC_I2C_BUS_WATCHDOG_H
#define MODUBOTIC_I2C_BUS_WATCHDOG_H

#include <Arduino.h>

void setupI2cBusWatchdog();
void updateI2cBusWatchdog(Stream &telemetryStream);
void scanI2cBusWatchdogNow(Stream &telemetryStream, bool forcePublish = false);
bool i2cBusHasGy33();
bool i2cBusHasEnvironmentSensor();
const char *getI2cBusSignature();

#endif
