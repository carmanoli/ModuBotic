#include <Arduino.h>
#include <string.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "lcd_display.h"

const byte LCD_ADDRESS_PRIMARY = 0x27;
const byte LCD_ADDRESS_SECONDARY = 0x3F;
const byte LCD_COLUMNS = 16;
const byte LCD_ROWS = 2;
const unsigned long LCD_REFRESH_MS = 1000;

LiquidCrystal_I2C lcd27(LCD_ADDRESS_PRIMARY, LCD_COLUMNS, LCD_ROWS);
LiquidCrystal_I2C lcd3f(LCD_ADDRESS_SECONDARY, LCD_COLUMNS, LCD_ROWS);
LiquidCrystal_I2C *lcd = &lcd27;

bool lcdEnabled = false;
bool lcdAvailable = false;
bool lcdInitialized = false;
bool i2cStarted = false;
byte activeLcdAddress = 0x00;
unsigned long lastLcdRefreshAt = 0;

bool i2cAddressResponds(byte address);
bool detectLcd();
void startI2cIfNeeded();

void setupLcd() {
  // Intentionally empty: LCD must not touch I2C during boot.
}

void updateLcd(const String &lastCommand) {
  (void)lastCommand;
  // Intentionally empty while isolating the eyes startup.
}

void updateLcdTelemetry(float temperatureC, float humidity, bool readingOk) {
  if (!lcdEnabled || !lcdAvailable || !lcdInitialized) {
    return;
  }

  unsigned long now = millis();
  if (now - lastLcdRefreshAt < LCD_REFRESH_MS) {
    return;
  }

  lastLcdRefreshAt = now;

  lcd->setCursor(0, 0);
  lcd->print("ModuBotica      ");

  lcd->setCursor(0, 1);
  if (!readingOk) {
    lcd->print("Sensor sem dados");
    return;
  }

  char line[LCD_COLUMNS + 1];
  snprintf(line, sizeof(line), "T:%4.1fC H:%3.0f%%", temperatureC, humidity);
  lcd->print(line);
  for (int i = strlen(line); i < LCD_COLUMNS; i++) {
    lcd->print(' ');
  }
}

void enableLcd(bool enabled) {
  startI2cIfNeeded();

  if (!detectLcd()) {
    lcdEnabled = false;
    return;
  }

  if (!lcdInitialized) {
    lcd->init();
    lcdInitialized = true;
  }

  lcdEnabled = enabled;

  if (lcdEnabled) {
    lcd->backlight();
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print("ModuBotica");
    lcd->setCursor(0, 1);
    lcd->print("I2C a ler...");
    lastLcdRefreshAt = 0;
  } else {
    lcd->clear();
    lcd->noBacklight();
  }
}

bool isLcdEnabled() {
  return lcdEnabled;
}

bool isLcdAvailable() {
  return lcdAvailable;
}

byte getLcdAddress() {
  return activeLcdAddress;
}

void startI2cIfNeeded() {
  if (i2cStarted) {
    return;
  }

  Wire.begin();
  i2cStarted = true;
  delay(20);
}

bool detectLcd() {
  if (lcdAvailable) {
    return true;
  }

  if (i2cAddressResponds(LCD_ADDRESS_PRIMARY)) {
    lcd = &lcd27;
    activeLcdAddress = LCD_ADDRESS_PRIMARY;
    lcdAvailable = true;
    return true;
  }

  if (i2cAddressResponds(LCD_ADDRESS_SECONDARY)) {
    lcd = &lcd3f;
    activeLcdAddress = LCD_ADDRESS_SECONDARY;
    lcdAvailable = true;
    return true;
  }

  return false;
}

bool i2cAddressResponds(byte address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}
