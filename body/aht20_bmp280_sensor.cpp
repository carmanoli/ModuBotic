#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "aht20_bmp280_sensor.h"

const byte AHT20_ADDRESS = 0x38;
const byte BMP280_ADDRESS_PRIMARY = 0x76;
const byte BMP280_ADDRESS_SECONDARY = 0x77;
const byte BMP280_CHIP_ID_REG = 0xD0;
const byte BMP280_CHIP_ID = 0x58;
const byte BMP280_CALIB_START = 0x88;
const byte BMP280_CTRL_MEAS = 0xF4;
const byte BMP280_CONFIG = 0xF5;
const byte BMP280_DATA_START = 0xF7;
const unsigned long ENV_READ_INTERVAL_MS = 2500;
const unsigned long ENV_RETRY_INTERVAL_MS = 1200;
const unsigned long ENV_I2C_CLOCK_HZ = 50000;
const int I2C_SUSPICIOUS_DEVICE_COUNT = 20;

struct Bmp280Calibration {
  uint16_t digT1;
  int16_t digT2;
  int16_t digT3;
  uint16_t digP1;
  int16_t digP2;
  int16_t digP3;
  int16_t digP4;
  int16_t digP5;
  int16_t digP6;
  int16_t digP7;
  int16_t digP8;
  int16_t digP9;
};

bool envEnabled = false;
bool envAvailable = false;
bool envInitialized = false;
bool envI2cStarted = false;
bool envReadingRequested = false;
bool envContinuousMode = false;
bool envReadingUpdated = false;
bool envReadingOk = false;
bool aht20Available = false;
bool bmp280Available = false;
byte bmp280Address = 0;
unsigned long lastEnvReadAt = 0;
unsigned long lastEnvAttemptAt = 0;
float lastAht20TemperatureC = NAN;
float lastAht20Humidity = NAN;
float lastBmp280TemperatureC = NAN;
float lastBmp280PressureHpa = NAN;
Bmp280Calibration bmpCal;
int32_t bmpTfine = 0;
const char *lastEnvError = "sem erro";

void startEnvI2cIfNeeded();
bool envAddressResponds(byte address);
bool detectAht20Bmp280();
bool initAht20();
bool initBmp280();
bool readAht20();
bool readBmp280();
bool readBytes(byte address, byte reg, byte *data, byte length);
bool writeByte(byte address, byte reg, byte value);
uint16_t u16le(const byte *data, byte index);
int16_t s16le(const byte *data, byte index);
void resetEnvReadingState();
void setEnvError(const char *message);

void setupAht20Bmp280Sensor() {
  // I2C environmental module stays off until ENV_ON, ENV_READ or ENV_SCAN.
}

void updateAht20Bmp280Sensor() {
  envReadingUpdated = false;

  if (!envEnabled) {
    return;
  }

  if (!envInitialized) {
    unsigned long now = millis();
    if (!envReadingRequested && now - lastEnvAttemptAt < ENV_RETRY_INTERVAL_MS) {
      return;
    }
    lastEnvAttemptAt = now;

    startEnvI2cIfNeeded();
    if (!detectAht20Bmp280()) {
      envReadingOk = false;
      envReadingUpdated = true;
      envEnabled = false;
      envContinuousMode = false;
      envReadingRequested = false;
      Serial.println("AHT20/BMP280: ausente, leitura continua desligada");
      return;
    }

    bool ahtOk = !aht20Available || initAht20();
    bool bmpOk = !bmp280Available || initBmp280();
    envInitialized = ahtOk && bmpOk && (aht20Available || bmp280Available);
    if (!envInitialized) {
      envReadingOk = false;
      envReadingUpdated = true;
      envEnabled = false;
      envContinuousMode = false;
      envReadingRequested = false;
      Serial.println("AHT20/BMP280: inicializacao falhou, leitura continua desligada");
      return;
    }
  }

  unsigned long now = millis();
  if (!envReadingRequested && now - lastEnvReadAt < ENV_READ_INTERVAL_MS) {
    return;
  }

  lastEnvReadAt = now;
  envReadingRequested = false;

  bool ok = true;
  if (aht20Available) {
    ok = readAht20() && ok;
  }
  if (bmp280Available) {
    ok = readBmp280() && ok;
  }

  envReadingOk = ok;
  envReadingUpdated = true;
  if (ok) {
    setEnvError("sem erro");
  }

  if (!envContinuousMode) {
    envEnabled = false;
  }
}

void printAht20Bmp280ReadingToSerial() {
  if (!envReadingUpdated) {
    return;
  }

  Serial.print("AHT20/BMP280: ");
  if (!envReadingOk) {
    Serial.print("sem leitura valida (");
    Serial.print(lastEnvError);
    Serial.println(")");
    return;
  }

  if (aht20Available) {
    Serial.print("AHT20 T=");
    Serial.print(lastAht20TemperatureC, 1);
    Serial.print(" C H=");
    Serial.print(lastAht20Humidity, 1);
    Serial.print(" %");
  }

  if (bmp280Available) {
    if (aht20Available) {
      Serial.print(" | ");
    }
    Serial.print("BMP280 T=");
    Serial.print(lastBmp280TemperatureC, 1);
    Serial.print(" C P=");
    Serial.print(lastBmp280PressureHpa, 1);
    Serial.print(" hPa addr=0x");
    Serial.print(bmp280Address, HEX);
  }

  Serial.println();
}

void printAht20Bmp280TelemetryToStream(Stream &stream) {
  if (!envReadingUpdated || !envReadingOk) {
    return;
  }

  stream.print("env=");
  if (aht20Available) {
    stream.print(lastAht20TemperatureC, 1);
    stream.print(",");
    stream.print(lastAht20Humidity, 1);
  } else {
    stream.print(",");
  }

  stream.print(",");
  if (bmp280Available) {
    stream.print(lastBmp280TemperatureC, 1);
    stream.print(",");
    stream.print(lastBmp280PressureHpa, 1);
    stream.print(",");
    stream.print(bmp280Address, HEX);
  } else {
    stream.print(",,");
  }
  stream.println();
}

void clearAht20Bmp280ReadingUpdated() {
  envReadingUpdated = false;
}

void scanAht20Bmp280I2cBus() {
  startEnvI2cIfNeeded();

  Serial.println("I2C scan:");
  int found = 0;
  for (byte address = 1; address < 127; address++) {
    if (envAddressResponds(address)) {
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
    Serial.println("I2C: barramento suspeito; muitos enderecos indica SDA/SCL errado, sem GND comum ou linhas soltas");
    return;
  }

  detectAht20Bmp280();
  Serial.print("AHT20 0x38=");
  Serial.print(aht20Available ? "sim" : "nao");
  Serial.print(" BMP280=");
  if (bmp280Available) {
    Serial.print("0x");
    Serial.println(bmp280Address, HEX);
  } else {
    Serial.println("nao");
  }
}

void requestAht20Bmp280Reading() {
  resetEnvReadingState();
  envEnabled = true;
  envContinuousMode = false;
  envReadingRequested = true;
  Serial.println("AHT20/BMP280: leitura pedida");
}

void enableAht20Bmp280Sensor(bool enabled) {
  resetEnvReadingState();
  envEnabled = enabled;
  envContinuousMode = enabled;

  if (!envEnabled) {
    Serial.println("AHT20/BMP280: desligado");
    return;
  }

  startEnvI2cIfNeeded();
  if (!detectAht20Bmp280()) {
    Serial.println("AHT20/BMP280: nao encontrado no I2C");
    scanAht20Bmp280I2cBus();
    return;
  }

  Serial.print("AHT20/BMP280: ON AHT20=");
  Serial.print(aht20Available ? "sim" : "nao");
  Serial.print(" BMP280=");
  if (bmp280Available) {
    Serial.print("0x");
    Serial.println(bmp280Address, HEX);
  } else {
    Serial.println("nao");
  }
}

bool isAht20Bmp280SensorEnabled() {
  return envEnabled;
}

bool isAht20Bmp280SensorAvailable() {
  return envAvailable;
}

bool hasAht20Bmp280Reading() {
  return envReadingUpdated;
}

bool isAht20Bmp280ReadingOk() {
  return envReadingOk;
}

float getAht20TemperatureC() {
  return lastAht20TemperatureC;
}

float getAht20Humidity() {
  return lastAht20Humidity;
}

float getBmp280TemperatureC() {
  return lastBmp280TemperatureC;
}

float getBmp280PressureHpa() {
  return lastBmp280PressureHpa;
}

void startEnvI2cIfNeeded() {
  if (envI2cStarted) {
    return;
  }

  Wire.begin();
  Wire.setClock(ENV_I2C_CLOCK_HZ);
  envI2cStarted = true;
  delay(20);
}

bool envAddressResponds(byte address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool detectAht20Bmp280() {
  aht20Available = envAddressResponds(AHT20_ADDRESS);
  bmp280Available = false;
  bmp280Address = 0;

  byte id = 0;
  if (envAddressResponds(BMP280_ADDRESS_PRIMARY) && readBytes(BMP280_ADDRESS_PRIMARY, BMP280_CHIP_ID_REG, &id, 1) && id == BMP280_CHIP_ID) {
    bmp280Available = true;
    bmp280Address = BMP280_ADDRESS_PRIMARY;
  } else if (envAddressResponds(BMP280_ADDRESS_SECONDARY) && readBytes(BMP280_ADDRESS_SECONDARY, BMP280_CHIP_ID_REG, &id, 1) && id == BMP280_CHIP_ID) {
    bmp280Available = true;
    bmp280Address = BMP280_ADDRESS_SECONDARY;
  }

  envAvailable = aht20Available || bmp280Available;
  if (!envAvailable) {
    setEnvError("enderecos 0x38 e 0x76/0x77 nao encontrados");
  }
  return envAvailable;
}

bool initAht20() {
  Wire.beginTransmission(AHT20_ADDRESS);
  Wire.write(0xBE);
  Wire.write(0x08);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    setEnvError("falha ao inicializar AHT20");
    return false;
  }

  delay(10);
  return true;
}

bool initBmp280() {
  byte data[24];
  if (!readBytes(bmp280Address, BMP280_CALIB_START, data, 24)) {
    setEnvError("falha ao ler calibracao BMP280");
    return false;
  }

  bmpCal.digT1 = u16le(data, 0);
  bmpCal.digT2 = s16le(data, 2);
  bmpCal.digT3 = s16le(data, 4);
  bmpCal.digP1 = u16le(data, 6);
  bmpCal.digP2 = s16le(data, 8);
  bmpCal.digP3 = s16le(data, 10);
  bmpCal.digP4 = s16le(data, 12);
  bmpCal.digP5 = s16le(data, 14);
  bmpCal.digP6 = s16le(data, 16);
  bmpCal.digP7 = s16le(data, 18);
  bmpCal.digP8 = s16le(data, 20);
  bmpCal.digP9 = s16le(data, 22);

  if (!writeByte(bmp280Address, BMP280_CONFIG, 0xA0) ||
      !writeByte(bmp280Address, BMP280_CTRL_MEAS, 0x27)) {
    setEnvError("falha ao configurar BMP280");
    return false;
  }

  delay(10);
  return true;
}

bool readAht20() {
  Wire.beginTransmission(AHT20_ADDRESS);
  Wire.write(0xAC);
  Wire.write(0x33);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    setEnvError("falha ao pedir leitura AHT20");
    return false;
  }

  delay(80);
  byte received = Wire.requestFrom(AHT20_ADDRESS, (byte)6);
  if (received < 6 || Wire.available() < 6) {
    setEnvError("resposta curta do AHT20");
    return false;
  }

  byte data[6];
  for (byte i = 0; i < 6; i++) {
    data[i] = Wire.read();
  }

  if ((data[0] & 0x80) != 0) {
    setEnvError("AHT20 ocupado");
    return false;
  }

  uint32_t rawHumidity = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
  uint32_t rawTemperature = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
  lastAht20Humidity = ((float)rawHumidity * 100.0) / 1048576.0;
  lastAht20TemperatureC = (((float)rawTemperature * 200.0) / 1048576.0) - 50.0;
  return true;
}

bool readBmp280() {
  byte data[6];
  if (!readBytes(bmp280Address, BMP280_DATA_START, data, 6)) {
    setEnvError("falha ao ler dados BMP280");
    return false;
  }

  int32_t adcP = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | (data[2] >> 4);
  int32_t adcT = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | (data[5] >> 4);

  int32_t var1 = ((((adcT >> 3) - ((int32_t)bmpCal.digT1 << 1))) * ((int32_t)bmpCal.digT2)) >> 11;
  int32_t var2 = (((((adcT >> 4) - ((int32_t)bmpCal.digT1)) * ((adcT >> 4) - ((int32_t)bmpCal.digT1))) >> 12) * ((int32_t)bmpCal.digT3)) >> 14;
  bmpTfine = var1 + var2;
  lastBmp280TemperatureC = ((bmpTfine * 5 + 128) >> 8) / 100.0;

  int64_t pvar1 = ((int64_t)bmpTfine) - 128000;
  int64_t pvar2 = pvar1 * pvar1 * (int64_t)bmpCal.digP6;
  pvar2 = pvar2 + ((pvar1 * (int64_t)bmpCal.digP5) << 17);
  pvar2 = pvar2 + (((int64_t)bmpCal.digP4) << 35);
  pvar1 = ((pvar1 * pvar1 * (int64_t)bmpCal.digP3) >> 8) + ((pvar1 * (int64_t)bmpCal.digP2) << 12);
  pvar1 = (((((int64_t)1) << 47) + pvar1)) * ((int64_t)bmpCal.digP1) >> 33;

  if (pvar1 == 0) {
    setEnvError("calibracao BMP280 invalida");
    return false;
  }

  int64_t pressure = 1048576 - adcP;
  pressure = (((pressure << 31) - pvar2) * 3125) / pvar1;
  pvar1 = (((int64_t)bmpCal.digP9) * (pressure >> 13) * (pressure >> 13)) >> 25;
  pvar2 = (((int64_t)bmpCal.digP8) * pressure) >> 19;
  pressure = ((pressure + pvar1 + pvar2) >> 8) + (((int64_t)bmpCal.digP7) << 4);
  lastBmp280PressureHpa = (pressure / 256.0) / 100.0;
  return true;
}

bool readBytes(byte address, byte reg, byte *data, byte length) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  byte status = Wire.endTransmission(false);
  if (status != 0) {
    return false;
  }

  byte received = Wire.requestFrom(address, length);
  if (received < length) {
    while (Wire.available() > 0) {
      Wire.read();
    }
    return false;
  }

  for (byte i = 0; i < length; i++) {
    data[i] = Wire.read();
  }

  return true;
}

bool writeByte(byte address, byte reg, byte value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

uint16_t u16le(const byte *data, byte index) {
  return (uint16_t)data[index] | ((uint16_t)data[index + 1] << 8);
}

int16_t s16le(const byte *data, byte index) {
  return (int16_t)u16le(data, index);
}

void resetEnvReadingState() {
  envInitialized = false;
  envReadingRequested = false;
  envContinuousMode = false;
  envReadingUpdated = false;
  envReadingOk = false;
  lastEnvReadAt = 0;
  lastEnvAttemptAt = 0;
  lastAht20TemperatureC = NAN;
  lastAht20Humidity = NAN;
  lastBmp280TemperatureC = NAN;
  lastBmp280PressureHpa = NAN;
  setEnvError("sem erro");
}

void setEnvError(const char *message) {
  lastEnvError = message;
}
