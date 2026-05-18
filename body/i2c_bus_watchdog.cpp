#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include "i2c_bus_watchdog.h"

const unsigned long I2C_WATCHDOG_BOOT_DELAY_MS = 2500;
const unsigned long I2C_WATCHDOG_SCAN_INTERVAL_MS = 5000;
const unsigned long I2C_WATCHDOG_CLOCK_HZ = 50000;
const byte I2C_MAX_DEVICES = 24;
const byte I2C_WATCHDOG_ADDRESSES_PER_LOOP = 4;

bool i2cWatchdogStarted = false;
bool i2cWatchdogReportedOnce = false;
bool i2cDetectedGy33 = false;
bool i2cDetectedEnvironment = false;
bool i2cWatchdogScanInProgress = false;
byte i2cWatchdogNextAddress = 1;
byte i2cWatchdogAddressCount = 0;
byte i2cWatchdogAddresses[I2C_MAX_DEVICES];
unsigned long lastI2cWatchdogScanAt = 0;
char lastI2cSignature[420] = "";

void startI2cWatchdogIfNeeded();
bool i2cWatchdogAddressResponds(byte address);
byte scanI2cAddresses(byte *addresses, byte maxAddresses);
const char *identifyI2cDevice(byte address);
void buildI2cSignature(const byte *addresses, byte count, char *buffer, size_t size);
void publishI2cDevices(Stream &telemetryStream, const char *signature);
void updateKnownI2cDeviceFlags(const byte *addresses, byte count);

void setupI2cBusWatchdog() {
  lastI2cWatchdogScanAt = millis();
}

void updateI2cBusWatchdog(Stream &telemetryStream) {
  unsigned long now = millis();

  if (now < I2C_WATCHDOG_BOOT_DELAY_MS) {
    return;
  }

  if (!i2cWatchdogScanInProgress && i2cWatchdogReportedOnce && now - lastI2cWatchdogScanAt < I2C_WATCHDOG_SCAN_INTERVAL_MS) {
    return;
  }

  if (!i2cWatchdogScanInProgress) {
    startI2cWatchdogIfNeeded();
    i2cWatchdogScanInProgress = true;
    i2cWatchdogNextAddress = 1;
    i2cWatchdogAddressCount = 0;
  }

  byte scanned = 0;
  while (i2cWatchdogNextAddress < 127 && scanned < I2C_WATCHDOG_ADDRESSES_PER_LOOP) {
    if (i2cWatchdogAddressResponds(i2cWatchdogNextAddress)) {
      if (i2cWatchdogAddressCount < I2C_MAX_DEVICES) {
        i2cWatchdogAddresses[i2cWatchdogAddressCount] = i2cWatchdogNextAddress;
      }
      i2cWatchdogAddressCount++;
    }

    i2cWatchdogNextAddress++;
    scanned++;
  }

  if (i2cWatchdogNextAddress < 127) {
    return;
  }

  char signature[sizeof(lastI2cSignature)];
  byte count = i2cWatchdogAddressCount > I2C_MAX_DEVICES ? I2C_MAX_DEVICES : i2cWatchdogAddressCount;

  updateKnownI2cDeviceFlags(i2cWatchdogAddresses, count);
  buildI2cSignature(i2cWatchdogAddresses, count, signature, sizeof(signature));

  if (!i2cWatchdogReportedOnce || strcmp(signature, lastI2cSignature) != 0) {
    strncpy(lastI2cSignature, signature, sizeof(lastI2cSignature) - 1);
    lastI2cSignature[sizeof(lastI2cSignature) - 1] = '\0';
    publishI2cDevices(telemetryStream, signature);
  }

  i2cWatchdogReportedOnce = true;
  i2cWatchdogScanInProgress = false;
  lastI2cWatchdogScanAt = now;
}

void scanI2cBusWatchdogNow(Stream &telemetryStream, bool forcePublish) {
  byte addresses[I2C_MAX_DEVICES];
  byte count = scanI2cAddresses(addresses, I2C_MAX_DEVICES);
  char signature[sizeof(lastI2cSignature)];

  updateKnownI2cDeviceFlags(addresses, count);
  buildI2cSignature(addresses, count, signature, sizeof(signature));

  if (forcePublish || !i2cWatchdogReportedOnce || strcmp(signature, lastI2cSignature) != 0) {
    strncpy(lastI2cSignature, signature, sizeof(lastI2cSignature) - 1);
    lastI2cSignature[sizeof(lastI2cSignature) - 1] = '\0';
    i2cWatchdogReportedOnce = true;
    publishI2cDevices(telemetryStream, signature);
  }
}

bool i2cBusHasGy33() {
  return i2cDetectedGy33;
}

bool i2cBusHasEnvironmentSensor() {
  return i2cDetectedEnvironment;
}

const char *getI2cBusSignature() {
  return lastI2cSignature;
}

void startI2cWatchdogIfNeeded() {
  if (i2cWatchdogStarted) {
    return;
  }

  Wire.begin();
  Wire.setClock(I2C_WATCHDOG_CLOCK_HZ);
  i2cWatchdogStarted = true;
  delay(20);
}

bool i2cWatchdogAddressResponds(byte address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

byte scanI2cAddresses(byte *addresses, byte maxAddresses) {
  startI2cWatchdogIfNeeded();

  byte count = 0;
  for (byte address = 1; address < 127; address++) {
    if (!i2cWatchdogAddressResponds(address)) {
      continue;
    }

    if (count < maxAddresses) {
      addresses[count] = address;
    }
    count++;
  }

  return count > maxAddresses ? maxAddresses : count;
}

const char *identifyI2cDevice(byte address) {
  switch (address) {
    case 0x27:
    case 0x3F:
      return "LCD_I2C";
    case 0x29:
      return "TCS34725_GY33";
    case 0x38:
      return "AHT20_AHT21";
    case 0x5A:
      return "GY33_CONTROLLER";
    case 0x68:
      return "MPU_RTC";
    case 0x76:
    case 0x77:
      return "BMP_BME280";
    default:
      return "UNKNOWN";
  }
}

void buildI2cSignature(const byte *addresses, byte count, char *buffer, size_t size) {
  buffer[0] = '\0';

  if (count == 0) {
    snprintf(buffer, size, "none");
    return;
  }

  for (byte i = 0; i < count; i++) {
    char item[34];
    snprintf(item, sizeof(item), "%02X:%s", addresses[i], identifyI2cDevice(addresses[i]));

    size_t used = strlen(buffer);
    if (i > 0 && used + 1 < size) {
      strncat(buffer, ",", size - used - 1);
    }

    used = strlen(buffer);
    if (used + 1 < size) {
      strncat(buffer, item, size - used - 1);
    }
  }
}

void publishI2cDevices(Stream &telemetryStream, const char *signature) {
  Serial.print("I2C WATCHDOG: ");
  Serial.println(signature);

  telemetryStream.print("i2c=");
  telemetryStream.println(signature);
}

void updateKnownI2cDeviceFlags(const byte *addresses, byte count) {
  i2cDetectedGy33 = false;
  i2cDetectedEnvironment = false;

  for (byte i = 0; i < count; i++) {
    if (addresses[i] == 0x29 || addresses[i] == 0x5A) {
      i2cDetectedGy33 = true;
    }
    if (addresses[i] == 0x38 || addresses[i] == 0x76 || addresses[i] == 0x77) {
      i2cDetectedEnvironment = true;
    }
  }
}
