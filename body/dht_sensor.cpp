#include <Arduino.h>
#include <math.h>
#include <DHT.h>
#include "dht_sensor.h"

const int DHT_LEFT_PIN = 2;
const int DHT_RIGHT_PIN = 3;
const int DHT_DIRECT_D7_PIN = 7;
const int DHT_DIRECT_D12_PIN = 12;
const int DHT_DIRECT_D13_PIN = 13;
const int DHT_TYPE = DHT22;
const unsigned long DHT_POWERUP_DELAY_MS = 2000;
const unsigned long DHT_READ_INTERVAL_MS = 2500;
const unsigned long DHT_STATUS_INTERVAL_MS = 5000;

DHT dhtLeft(DHT_LEFT_PIN, DHT_TYPE);
DHT dhtRight(DHT_RIGHT_PIN, DHT_TYPE);
DHT dhtDirectD7(DHT_DIRECT_D7_PIN, DHT_TYPE);
DHT dhtDirectD12(DHT_DIRECT_D12_PIN, DHT_TYPE);
DHT dhtDirectD13(DHT_DIRECT_D13_PIN, DHT_TYPE);
DHT *activeDht = nullptr;
int activeDhtPin = -1;
const char *activeDhtName = "desligado";

unsigned long lastDhtReadAt = 0;
unsigned long dhtStartedAt = 0;
unsigned long lastDhtStatusAt = 0;
float lastTemperatureC = NAN;
float lastHumidity = NAN;
bool dhtHasReading = false;
bool dhtReadingOk = false;
bool dhtReadingUpdated = false;
int consecutiveDhtErrors = 0;

void resetDhtReadingState();
void prepareDhtPin(int pin);
void printDhtStatusIfDue(const char *status);

void setupDhtSensor() {
  prepareDhtPin(DHT_LEFT_PIN);
  prepareDhtPin(DHT_RIGHT_PIN);
  prepareDhtPin(DHT_DIRECT_D7_PIN);
  prepareDhtPin(DHT_DIRECT_D12_PIN);
  prepareDhtPin(DHT_DIRECT_D13_PIN);
  dhtLeft.begin();
  dhtRight.begin();
  dhtDirectD7.begin();
  dhtDirectD12.begin();
  dhtDirectD13.begin();
  dhtStartedAt = millis();
}

void updateDhtSensor() {
  dhtReadingUpdated = false;

  unsigned long now = millis();
  if (activeDht == nullptr) {
    printDhtStatusIfDue("desligado");
    return;
  }

  if (now - dhtStartedAt < DHT_POWERUP_DELAY_MS) {
    printDhtStatusIfDue("a estabilizar");
    return;
  }

  if (dhtHasReading && now - lastDhtReadAt < DHT_READ_INTERVAL_MS) {
    return;
  }

  lastDhtReadAt = now;

  float humidity = activeDht->readHumidity();
  float temperatureC = activeDht->readTemperature();

  dhtHasReading = true;
  dhtReadingUpdated = true;
  if (isnan(humidity) || isnan(temperatureC)) {
    dhtReadingOk = false;
    consecutiveDhtErrors++;
    return;
  }

  consecutiveDhtErrors = 0;
  lastHumidity = humidity;
  lastTemperatureC = temperatureC;
  dhtReadingOk = true;
}

void printDhtReadingToSerial() {
  if (!dhtReadingUpdated) {
    return;
  }

  Serial.print("DHT22 ");
  Serial.print(activeDhtName);
  Serial.print(": ");
  if (!dhtReadingOk) {
    Serial.print("leitura invalida");
    if (consecutiveDhtErrors >= 3) {
      Serial.print(" (verificar S/VCC/GND; D13 pode falhar por LED/SCK; D12 reserva o braco esquerdo)");
    }
    Serial.println();
    return;
  }

  Serial.print("T=");
  Serial.print(lastTemperatureC, 1);
  Serial.print(" C  H=");
  Serial.print(lastHumidity, 1);
  Serial.print(" %");

  if (lastHumidity >= 99.5 || lastTemperatureC >= 45.0) {
    Serial.print("  (suspeito: verificar pull-up, ligacoes e calor perto do sensor)");
  }

  Serial.println();
}

void printDhtTelemetryToStream(Stream &stream) {
  if (!dhtReadingOk) {
    return;
  }

  stream.print("dht=");
  stream.print(lastTemperatureC, 1);
  stream.print(",");
  stream.print(lastHumidity, 1);
  stream.print(",");
  stream.print(activeDhtPin);
}

void clearDhtReadingUpdated() {
  dhtReadingUpdated = false;
}

void selectDhtLeftArm() {
  activeDht = &dhtLeft;
  activeDhtPin = DHT_LEFT_PIN;
  activeDhtName = "braco esquerdo D2";
  dhtLeft.begin();
  resetDhtReadingState();
  Serial.println("DHT22: selecionado braco esquerdo D2");
}

void selectDhtRightArm() {
  activeDht = &dhtRight;
  activeDhtPin = DHT_RIGHT_PIN;
  activeDhtName = "braco direito D3";
  dhtRight.begin();
  resetDhtReadingState();
  Serial.println("DHT22: selecionado braco direito D3");
}

void selectDhtDirectD7() {
  activeDht = &dhtDirectD7;
  activeDhtPin = DHT_DIRECT_D7_PIN;
  activeDhtName = "direto D7";
  prepareDhtPin(DHT_DIRECT_D7_PIN);
  dhtDirectD7.begin();
  resetDhtReadingState();
  Serial.println("DHT22: selecionado direto D7");
}

void selectDhtDirectD12() {
  activeDht = &dhtDirectD12;
  activeDhtPin = DHT_DIRECT_D12_PIN;
  activeDhtName = "direto D12";
  prepareDhtPin(DHT_DIRECT_D12_PIN);
  dhtDirectD12.begin();
  resetDhtReadingState();
  Serial.println("DHT22: selecionado direto D12 (braço esquerdo fica reservado/desativado)");
}

void selectDhtDirectD13() {
  activeDht = &dhtDirectD13;
  activeDhtPin = DHT_DIRECT_D13_PIN;
  activeDhtName = "direto D13";
  prepareDhtPin(DHT_DIRECT_D13_PIN);
  dhtDirectD13.begin();
  resetDhtReadingState();
  Serial.println("DHT22: selecionado direto D13 (D13 partilha LED/SCK; se falhar usa D7)");
}

void disableDhtSensor() {
  activeDht = nullptr;
  activeDhtPin = -1;
  activeDhtName = "desligado";
  resetDhtReadingState();
  Serial.println("DHT22: desligado");
}

void resetDhtReadingState() {
  lastDhtReadAt = 0;
  dhtStartedAt = millis();
  lastDhtStatusAt = 0;
  lastTemperatureC = NAN;
  lastHumidity = NAN;
  dhtHasReading = false;
  dhtReadingOk = false;
  dhtReadingUpdated = false;
  consecutiveDhtErrors = 0;
}

void prepareDhtPin(int pin) {
  pinMode(pin, INPUT_PULLUP);
}

void printDhtStatusIfDue(const char *status) {
  unsigned long now = millis();
  if (now - lastDhtStatusAt < DHT_STATUS_INTERVAL_MS) {
    return;
  }

  lastDhtStatusAt = now;
  Serial.print("DHT22 ");
  Serial.print(activeDhtName);
  Serial.print(": ");
  Serial.println(status);
}

bool isDhtUsingPin(int pin) {
  return activeDhtPin == pin;
}

bool hasDhtReading() {
  return dhtHasReading;
}

bool isDhtReadingOk() {
  return dhtReadingOk;
}

float getDhtTemperatureC() {
  return lastTemperatureC;
}

float getDhtHumidity() {
  return lastHumidity;
}
