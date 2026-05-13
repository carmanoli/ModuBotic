#include <Arduino.h>
#include <Wire.h>
#include "gy33_color_sensor.h"
#include "i2c_bus_watchdog.h"

const byte TCS34725_ADDRESS = 0x29;
const byte GY33_CONTROLLER_ADDRESS = 0x5A;
const byte GY33_CONTROLLER_CONFIG = 0x10;
const byte TCS34725_COMMAND_BIT = 0x80;
const byte TCS34725_COMMAND_AUTO_INCREMENT = 0x20;
const byte TCS34725_ENABLE = 0x00;
const byte TCS34725_ATIME = 0x01;
const byte TCS34725_AILTL = 0x04;
const byte TCS34725_AILTH = 0x05;
const byte TCS34725_PERS = 0x0C;
const byte TCS34725_CONTROL = 0x0F;
const byte TCS34725_ID = 0x12;
const byte TCS34725_STATUS = 0x13;
const byte TCS34725_CDATAL = 0x14;
const byte TCS34725_ENABLE_PON = 0x01;
const byte TCS34725_ENABLE_AEN = 0x02;
const byte TCS34725_ENABLE_AIEN = 0x10;
const byte TCS34725_STATUS_AVALID = 0x01;
const unsigned long GY33_READ_INTERVAL_MS = 1000;
const unsigned long GY33_TELEMETRY_INTERVAL_MS = 3000;
const unsigned long GY33_RETRY_INTERVAL_MS = 1000;
const unsigned long GY33_I2C_CLOCK_HZ = 50000;
const byte GY33_LED_INT_PIN = 13;
const int I2C_SUSPICIOUS_DEVICE_COUNT = 20;
const char *GY33_ERROR_NONE = "sem erro";

#if __has_include(<Adafruit_TCS34725.h>)
#include <Adafruit_TCS34725.h>
#define GY33_USE_ADAFRUIT_TCS34725 1
Adafruit_TCS34725 gy33Tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
#else
#define GY33_USE_ADAFRUIT_TCS34725 0
#endif

bool gy33Enabled = false;
bool gy33Available = false;
bool gy33DirectTcsMode = false;
bool gy33ControllerMode = false;
bool gy33Initialized = false;
bool gy33I2cStarted = false;
bool gy33AdafruitInitialized = false;
bool gy33ReadingUpdated = false;
bool gy33ReadingOk = false;
bool gy33ReadingRequested = false;
bool gy33ContinuousMode = false;
unsigned long lastGy33ReadAt = 0;
unsigned long lastGy33TelemetryAt = 0;
unsigned long lastGy33AttemptAt = 0;
uint16_t lastClear = 0;
uint16_t lastRed = 0;
uint16_t lastGreen = 0;
uint16_t lastBlue = 0;
uint16_t lastLux = 0;
uint16_t lastColorTemperature = 0;
byte lastRed8 = 0;
byte lastGreen8 = 0;
byte lastBlue8 = 0;
byte lastColorCode = 0;
const char *lastGy33Error = GY33_ERROR_NONE;
byte lastGy33FailRegister = 0;
byte lastGy33FailExpected = 0;
byte lastGy33FailReceived = 0;
byte lastGy33FailStatus = 0;

void startGy33I2cIfNeeded();
bool gy33AddressResponds(byte address);
bool detectGy33();
bool detectTcs34725Direct();
bool detectGy33Controller();
bool initTcs34725();
void setGy33IntPinLed(bool enabled);
void setTcs34725InterruptLed(bool enabled);
void writeTcs34725Register(byte reg, byte value);
byte readTcs34725Register(byte reg);
bool readTcs34725RegisterByte(byte reg, byte *value);
uint16_t readTcs34725Word(byte reg);
bool readTcs34725WordValue(byte reg, uint16_t *value);
bool readGy33ControllerRegisters();
bool readGy33ControllerBytes(byte reg, byte *data, byte length);
bool writeGy33ControllerByte(byte reg, byte value);
void printGy33AddressState();
void printGy33ControllerRead(byte reg, byte length);
void printGy33Bytes(byte *data, byte length);
void setGy33Error(const char *message);
void setGy33ReadError(const char *message, byte reg, byte expected, byte received, byte status);
byte scaleRawColorToByte(uint16_t value, uint16_t clear);
void resetGy33ReadingState();

void setupGy33ColorSensor() {
  // Sensor stays off until GY33_ON or GY33_SCAN is requested.
  setGy33IntPinLed(false);
}

void updateGy33ColorSensor() {
  gy33ReadingUpdated = false;

  if (!gy33Enabled) {
    return;
  }

  if (!gy33Initialized) {
    unsigned long now = millis();
    if (!gy33ReadingRequested && now - lastGy33AttemptAt < GY33_RETRY_INTERVAL_MS) {
      return;
    }
    lastGy33AttemptAt = now;

    startGy33I2cIfNeeded();
    if (!detectGy33()) {
      gy33ReadingOk = false;
      setGy33Error("nao encontrado no barramento I2C");
      gy33ReadingUpdated = true;
      if (!gy33ContinuousMode) {
        gy33Enabled = false;
        gy33ReadingRequested = false;
      }
      return;
    }

    gy33Initialized = gy33ControllerMode || initTcs34725();
    if (!gy33Initialized) {
      gy33ReadingOk = false;
      gy33ReadingUpdated = true;
      if (!gy33ContinuousMode) {
        gy33Enabled = false;
        gy33ReadingRequested = false;
      }
      return;
    }
  }

  unsigned long now = millis();
  if (!gy33ReadingRequested && now - lastGy33ReadAt < GY33_READ_INTERVAL_MS) {
    return;
  }

  lastGy33ReadAt = now;
  gy33ReadingRequested = false;

  if (gy33ControllerMode) {
    gy33ReadingOk = readGy33ControllerRegisters();
    gy33ReadingUpdated = true;
    return;
  }

#if GY33_USE_ADAFRUIT_TCS34725
  if (gy33AdafruitInitialized) {
    gy33Tcs.getRawData(&lastRed, &lastGreen, &lastBlue, &lastClear);
    lastLux = gy33Tcs.calculateLux(lastRed, lastGreen, lastBlue);
    lastColorTemperature = gy33Tcs.calculateColorTemperature(lastRed, lastGreen, lastBlue);
    lastRed8 = scaleRawColorToByte(lastRed, lastClear);
    lastGreen8 = scaleRawColorToByte(lastGreen, lastClear);
    lastBlue8 = scaleRawColorToByte(lastBlue, lastClear);
    gy33ReadingOk = true;
    setGy33Error(GY33_ERROR_NONE);
    gy33ReadingUpdated = true;

    if (!gy33ContinuousMode) {
      gy33Enabled = false;
    }
    return;
  }
#endif

  byte status = 0;
  if (!readTcs34725RegisterByte(TCS34725_STATUS, &status)) {
    gy33ReadingOk = false;
    gy33ReadingUpdated = true;
    return;
  }

  if ((status & TCS34725_STATUS_AVALID) == 0) {
    gy33ReadingOk = false;
    setGy33Error("TCS34725 sem dados validos");
    gy33ReadingUpdated = true;
    return;
  }

  if (!readTcs34725WordValue(TCS34725_CDATAL, &lastClear) ||
      !readTcs34725WordValue(TCS34725_CDATAL + 2, &lastRed) ||
      !readTcs34725WordValue(TCS34725_CDATAL + 4, &lastGreen) ||
      !readTcs34725WordValue(TCS34725_CDATAL + 6, &lastBlue)) {
    gy33ReadingOk = false;
    gy33ReadingUpdated = true;
    return;
  }

  lastRed8 = scaleRawColorToByte(lastRed, lastClear);
  lastGreen8 = scaleRawColorToByte(lastGreen, lastClear);
  lastBlue8 = scaleRawColorToByte(lastBlue, lastClear);
  gy33ReadingOk = true;
  setGy33Error(GY33_ERROR_NONE);
  gy33ReadingUpdated = true;

  if (!gy33ContinuousMode) {
    gy33Enabled = false;
  }
}

void printGy33ReadingToSerial() {
  if (!gy33ReadingUpdated) {
    return;
  }

  Serial.print("GY-33: ");
  if (!gy33ReadingOk) {
    Serial.print("sem leitura valida (");
    Serial.print(lastGy33Error);
    if (lastGy33FailExpected > 0) {
      Serial.print("; reg=0x");
      Serial.print(lastGy33FailRegister, HEX);
      Serial.print(" esperava=");
      Serial.print(lastGy33FailExpected);
      Serial.print(" recebeu=");
      Serial.print(lastGy33FailReceived);
      Serial.print(" status=");
      Serial.print(lastGy33FailStatus);
    }
    Serial.println(")");
    return;
  }

  Serial.print("C=");
  Serial.print(lastClear);
  Serial.print(" R=");
  Serial.print(lastRed);
  Serial.print(" G=");
  Serial.print(lastGreen);
  Serial.print(" B=");
  Serial.print(lastBlue);

  if (gy33ControllerMode) {
    Serial.print(" | RGB8=");
    Serial.print(lastRed8);
    Serial.print(",");
    Serial.print(lastGreen8);
    Serial.print(",");
    Serial.print(lastBlue8);
    Serial.print(" Lux=");
    Serial.print(lastLux);
    Serial.print(" CT=");
    Serial.print(lastColorTemperature);
    Serial.print(" color=0x");
    Serial.println(lastColorCode, HEX);
    return;
  }

  Serial.print(" | RGB8=");
  Serial.print(lastRed8);
  Serial.print(",");
  Serial.print(lastGreen8);
  Serial.print(",");
  Serial.print(lastBlue8);
  Serial.println();
}

void printGy33TelemetryToStream(Stream &stream) {
  if (!gy33ReadingUpdated || !gy33ReadingOk) {
    return;
  }

  unsigned long now = millis();
  if (now - lastGy33TelemetryAt < GY33_TELEMETRY_INTERVAL_MS) {
    return;
  }

  lastGy33TelemetryAt = now;

  stream.print("rgb=");
  stream.print(lastRed8);
  stream.print(",");
  stream.print(lastGreen8);
  stream.print(",");
  stream.print(lastBlue8);

  const char *i2cSignature = getI2cBusSignature();
  if (i2cSignature != NULL && i2cSignature[0] != '\0') {
    stream.print("&i2c=");
    stream.print(i2cSignature);
  }
}

void clearGy33ReadingUpdated() {
  gy33ReadingUpdated = false;
}

void scanGy33I2cBus() {
  startGy33I2cIfNeeded();

  Serial.println("I2C scan:");
  int found = 0;
  for (byte address = 1; address < 127; address++) {
    if (gy33AddressResponds(address)) {
      found++;
      if (found <= I2C_SUSPICIOUS_DEVICE_COUNT) {
        Serial.print("  encontrado 0x");
        if (address < 16) {
          Serial.print('0');
        }
        Serial.println(address, HEX);
      } else if (found == I2C_SUSPICIOUS_DEVICE_COUNT + 1) {
        Serial.println("  ... muitos enderecos a responder; scan abreviado");
      }
    }
  }

  if (found == 0) {
    Serial.println("  nenhum dispositivo encontrado");
    return;
  }

  Serial.print("  total=");
  Serial.println(found);

  if (found > I2C_SUSPICIOUS_DEVICE_COUNT) {
    Serial.println("I2C: barramento suspeito; muitos enderecos indica SDA/SCL errado, sem GND comum, linhas soltas ou modo I2C incorreto");
    return;
  }

  if (detectTcs34725Direct()) {
    Serial.println("GY-33: TCS34725 direto detectado em 0x29");
  }
  if (detectGy33Controller()) {
    Serial.println("GY-33: controlador do modulo detectado em 0x5A");
  }
}

void debugGy33ColorSensor() {
  startGy33I2cIfNeeded();
  scanGy33I2cBus();
  detectGy33();
  printGy33AddressState();

  if (!gy33DirectTcsMode && !gy33ControllerMode) {
    Serial.println("GY-33 DEBUG: ausente. Para 0x5A usa CT->SCL, DR->SDA, S0->GND, S1 solto; para 0x29 usa SCL/SDA do chip e S1->GND.");
  }

  if (gy33DirectTcsMode) {
    Serial.print("GY-33 DEBUG 0x29 ID=0x");
    Serial.print(readTcs34725Register(TCS34725_ID), HEX);
    Serial.print(" STATUS=0x");
    Serial.println(readTcs34725Register(TCS34725_STATUS), HEX);
  }

  if (gy33ControllerMode) {
    printGy33ControllerRead(0x00, 8);
    printGy33ControllerRead(0x08, 4);
    printGy33ControllerRead(0x0C, 3);
    printGy33ControllerRead(0x0F, 1);
  }
}

void requestGy33ColorReading() {
  resetGy33ReadingState();
  gy33Enabled = true;
  gy33ContinuousMode = false;
  gy33ReadingRequested = true;
  Serial.println("GY-33: leitura pedida");
}

void setGy33LedPower(byte power) {
  startGy33I2cIfNeeded();
  if (power > 10) {
    power = 10;
  }

  setGy33IntPinLed(power > 0);

  if (detectTcs34725Direct()) {
    setTcs34725InterruptLed(power > 0);
    Serial.print("GY-33 LED: ");
    Serial.println(power > 0 ? "ON por D13/INT + TCS34725 direto 0x29" : "OFF por D13/INT + TCS34725 direto 0x29");
    return;
  }

  if (!detectGy33Controller()) {
    Serial.println("GY-33 LED: modulo nao encontrado em 0x29 nem 0x5A");
    return;
  }

  byte config = 0xA0 - (power * 16);
  if (writeGy33ControllerByte(GY33_CONTROLLER_CONFIG, config)) {
    Serial.print("GY-33 LED: brilho ");
    Serial.print(power);
    if (power == 0) {
      Serial.print(" (minimo)");
    }
    Serial.println();
  } else {
    Serial.println("GY-33 LED: falha ao escrever configuracao");
  }
}

void enableGy33ColorSensor(bool enabled) {
  resetGy33ReadingState();
  gy33Enabled = enabled;
  gy33ContinuousMode = enabled;

  if (!gy33Enabled) {
    if (gy33DirectTcsMode) {
      setTcs34725InterruptLed(false);
    }
    setGy33IntPinLed(false);
    Serial.println("GY-33: desligado");
    return;
  }

  startGy33I2cIfNeeded();
  if (!detectGy33()) {
    Serial.println("GY-33: nao encontrado no I2C");
    scanGy33I2cBus();
    return;
  }

  printGy33AddressState();

  if (gy33DirectTcsMode) {
    Serial.println("GY-33: ON em modo TCS34725 direto 0x29");
    return;
  }

  if (gy33ControllerMode) {
    Serial.println("GY-33: ON em modo controlador do modulo 0x5A");
  }
}

bool isGy33ColorSensorEnabled() {
  return gy33Enabled;
}

bool isGy33ColorSensorAvailable() {
  return gy33Available;
}

bool hasGy33Reading() {
  return gy33ReadingUpdated;
}

bool isGy33ReadingOk() {
  return gy33ReadingOk;
}

byte getGy33Red8() {
  return lastRed8;
}

byte getGy33Green8() {
  return lastGreen8;
}

byte getGy33Blue8() {
  return lastBlue8;
}

void startGy33I2cIfNeeded() {
  if (gy33I2cStarted) {
    return;
  }

  Wire.begin();
  Wire.setClock(GY33_I2C_CLOCK_HZ);
  gy33I2cStarted = true;
  delay(20);
}

bool gy33AddressResponds(byte address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool detectGy33() {
  gy33DirectTcsMode = detectTcs34725Direct();
  gy33ControllerMode = gy33DirectTcsMode ? false : detectGy33Controller();
  gy33Available = gy33DirectTcsMode || gy33ControllerMode;
  if (!gy33Available && lastGy33Error == GY33_ERROR_NONE) {
    setGy33Error("0x29/0x5A nao validaram como GY-33");
  }
  return gy33Available;
}

bool detectTcs34725Direct() {
  if (!gy33AddressResponds(TCS34725_ADDRESS)) {
    return false;
  }

  for (byte attempt = 0; attempt < 3; attempt++) {
    byte id = 0;
    if (readTcs34725RegisterByte(TCS34725_ID, &id) && (id == 0x44 || id == 0x4D)) {
      return true;
    }
    delay(5);
  }

  setGy33Error("0x29 respondeu mas ID TCS34725 invalido");
  return false;
}

bool detectGy33Controller() {
  if (!gy33AddressResponds(GY33_CONTROLLER_ADDRESS)) {
    return false;
  }

  byte data[1];
  return readGy33ControllerBytes(0x0C, data, 1);
}

bool initTcs34725() {
#if GY33_USE_ADAFRUIT_TCS34725
  if (!gy33Tcs.begin()) {
    gy33AdafruitInitialized = false;
    setGy33Error("Adafruit_TCS34725 nao inicializou");
    return false;
  }

  gy33AdafruitInitialized = true;
  setGy33IntPinLed(true);
  setTcs34725InterruptLed(true);
  delay(60);
  return true;
#else
  byte id = 0;
  if (!readTcs34725RegisterByte(TCS34725_ID, &id) || (id != 0x44 && id != 0x4D)) {
    setGy33Error("ID TCS34725 inesperado");
    Serial.print("GY-33: ID inesperado 0x");
    Serial.println(id, HEX);
    return false;
  }

  writeTcs34725Register(TCS34725_ATIME, 0xEB);
  writeTcs34725Register(TCS34725_CONTROL, 0x01);
  writeTcs34725Register(TCS34725_ENABLE, TCS34725_ENABLE_PON);
  delay(3);
  setGy33IntPinLed(true);
  setTcs34725InterruptLed(true);
  delay(60);
  return true;
#endif
}

void setGy33IntPinLed(bool enabled) {
  if (enabled) {
    digitalWrite(GY33_LED_INT_PIN, LOW);
    pinMode(GY33_LED_INT_PIN, OUTPUT);
    return;
  }

  pinMode(GY33_LED_INT_PIN, INPUT);
}

void setTcs34725InterruptLed(bool enabled) {
#if GY33_USE_ADAFRUIT_TCS34725
  if (!gy33AdafruitInitialized) {
    gy33AdafruitInitialized = gy33Tcs.begin();
  }

  if (gy33AdafruitInitialized) {
    if (enabled) {
      gy33Tcs.setIntLimits(0xFFFF, 0x0000);
      gy33Tcs.clearInterrupt();
      gy33Tcs.setInterrupt(false);
    } else {
      gy33Tcs.setInterrupt(true);
    }
    return;
  }
#endif

  if (enabled) {
    // Some GY-33 boards wire the TCS34725 INT output to the onboard LED driver.
    // Force a low-threshold interrupt so INT asserts while RGBC readings run.
    writeTcs34725Register(TCS34725_ENABLE, TCS34725_ENABLE_PON);
    delay(3);
    writeTcs34725Register(TCS34725_AILTL, 0xFF);
    writeTcs34725Register(TCS34725_AILTH, 0xFF);
    writeTcs34725Register(TCS34725_PERS, 0x00);
    writeTcs34725Register(TCS34725_ENABLE, TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN | TCS34725_ENABLE_AIEN);
    return;
  }

  writeTcs34725Register(TCS34725_ENABLE, TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN);
}

void writeTcs34725Register(byte reg, byte value) {
  Wire.beginTransmission(TCS34725_ADDRESS);
  Wire.write(TCS34725_COMMAND_BIT | reg);
  Wire.write(value);
  Wire.endTransmission();
}

byte readTcs34725Register(byte reg) {
  byte value = 0;
  readTcs34725RegisterByte(reg, &value);
  return value;
}

bool readTcs34725RegisterByte(byte reg, byte *value) {
  Wire.beginTransmission(TCS34725_ADDRESS);
  Wire.write(TCS34725_COMMAND_BIT | reg);
  byte status = Wire.endTransmission(false);
  if (status != 0) {
    setGy33ReadError("falha ao selecionar registo 0x29", reg, 1, 0, status);
    return false;
  }

  byte received = Wire.requestFrom(TCS34725_ADDRESS, (byte)1);
  if (received < 1 || Wire.available() < 1) {
    setGy33ReadError("resposta curta do TCS34725 0x29", reg, 1, received, 0);
    return false;
  }

  *value = Wire.read();
  return true;
}

uint16_t readTcs34725Word(byte reg) {
  uint16_t value = 0;
  readTcs34725WordValue(reg, &value);
  return value;
}

bool readTcs34725WordValue(byte reg, uint16_t *value) {
  Wire.beginTransmission(TCS34725_ADDRESS);
  Wire.write(TCS34725_COMMAND_BIT | TCS34725_COMMAND_AUTO_INCREMENT | reg);
  byte status = Wire.endTransmission(false);
  if (status != 0) {
    setGy33ReadError("falha ao selecionar registo 16-bit 0x29", reg, 2, 0, status);
    return false;
  }

  byte received = Wire.requestFrom(TCS34725_ADDRESS, (byte)2);
  if (received < 2 || Wire.available() < 2) {
    setGy33ReadError("resposta curta 16-bit do TCS34725 0x29", reg, 2, received, 0);
    return false;
  }

  byte low = Wire.read();
  byte high = Wire.read();
  *value = ((uint16_t)high << 8) | low;
  return true;
}

bool readGy33ControllerRegisters() {
  byte data[8];
  if (!readGy33ControllerBytes(0x00, data, 8)) {
    return false;
  }

  lastRed = ((uint16_t)data[0] << 8) | data[1];
  lastGreen = ((uint16_t)data[2] << 8) | data[3];
  lastBlue = ((uint16_t)data[4] << 8) | data[5];
  lastClear = ((uint16_t)data[6] << 8) | data[7];

  if (readGy33ControllerBytes(0x08, data, 4)) {
    lastLux = ((uint16_t)data[0] << 8) | data[1];
    lastColorTemperature = ((uint16_t)data[2] << 8) | data[3];
  }

  if (readGy33ControllerBytes(0x0C, data, 3)) {
    lastRed8 = data[0];
    lastGreen8 = data[1];
    lastBlue8 = data[2];
  }

  if (readGy33ControllerBytes(0x0F, data, 1)) {
    lastColorCode = data[0];
  }

  setGy33Error(GY33_ERROR_NONE);
  return true;
}

bool readGy33ControllerBytes(byte reg, byte *data, byte length) {
  Wire.beginTransmission(GY33_CONTROLLER_ADDRESS);
  Wire.write(reg);
  byte status = Wire.endTransmission();
  if (status != 0) {
    setGy33ReadError("falha ao selecionar registo 0x5A", reg, length, 0, status);
    return false;
  }

  byte received = Wire.requestFrom(GY33_CONTROLLER_ADDRESS, length);
  if (received < length) {
    while (Wire.available() > 0) {
      Wire.read();
    }
    setGy33ReadError("resposta curta do controlador 0x5A", reg, length, received, 0);
    return false;
  }

  for (byte i = 0; i < length; i++) {
    data[i] = Wire.read();
  }

  return true;
}

bool writeGy33ControllerByte(byte reg, byte value) {
  Wire.beginTransmission(GY33_CONTROLLER_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  byte status = Wire.endTransmission();
  if (status != 0) {
    setGy33ReadError("falha ao escrever controlador 0x5A", reg, 1, 0, status);
    return false;
  }

  return true;
}

void printGy33AddressState() {
  Serial.print("GY-33 DEBUG: 0x29=");
  Serial.print(gy33DirectTcsMode ? "sim" : "nao");
  Serial.print(" 0x5A=");
  Serial.println(gy33ControllerMode ? "sim" : "nao");
}

void printGy33ControllerRead(byte reg, byte length) {
  byte data[8];
  Serial.print("GY-33 DEBUG reg 0x");
  Serial.print(reg, HEX);
  Serial.print(": ");
  if (!readGy33ControllerBytes(reg, data, length)) {
    Serial.print("falhou (");
    Serial.print(lastGy33Error);
    Serial.print("; esperava=");
    Serial.print(lastGy33FailExpected);
    Serial.print(" recebeu=");
    Serial.print(lastGy33FailReceived);
    Serial.print(" status=");
    Serial.print(lastGy33FailStatus);
    Serial.println(")");
    return;
  }

  printGy33Bytes(data, length);
}

void printGy33Bytes(byte *data, byte length) {
  for (byte i = 0; i < length; i++) {
    if (i > 0) {
      Serial.print(' ');
    }
    if (data[i] < 16) {
      Serial.print('0');
    }
    Serial.print(data[i], HEX);
  }
  Serial.println();
}

void setGy33Error(const char *message) {
  lastGy33Error = message;
  lastGy33FailRegister = 0;
  lastGy33FailExpected = 0;
  lastGy33FailReceived = 0;
  lastGy33FailStatus = 0;
}

void setGy33ReadError(const char *message, byte reg, byte expected, byte received, byte status) {
  lastGy33Error = message;
  lastGy33FailRegister = reg;
  lastGy33FailExpected = expected;
  lastGy33FailReceived = received;
  lastGy33FailStatus = status;
}

byte scaleRawColorToByte(uint16_t value, uint16_t clear) {
  if (clear == 0) {
    return value > 255 ? 255 : (byte)value;
  }

  unsigned long scaled = ((unsigned long)value * 255UL) / clear;
  return scaled > 255 ? 255 : (byte)scaled;
}

void resetGy33ReadingState() {
  gy33Initialized = false;
  gy33AdafruitInitialized = false;
  gy33ReadingUpdated = false;
  gy33ReadingOk = false;
  gy33ReadingRequested = false;
  gy33ContinuousMode = false;
  lastGy33ReadAt = 0;
  lastGy33TelemetryAt = 0;
  lastGy33AttemptAt = 0;
  lastClear = 0;
  lastRed = 0;
  lastGreen = 0;
  lastBlue = 0;
  lastLux = 0;
  lastColorTemperature = 0;
  lastRed8 = 0;
  lastGreen8 = 0;
  lastBlue8 = 0;
  lastColorCode = 0;
  setGy33Error(GY33_ERROR_NONE);
}
