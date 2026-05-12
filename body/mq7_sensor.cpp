#include <Arduino.h>
#include "mq7_sensor.h"

const int MQ7_LEFT_PIN = A0;
const int MQ7_RIGHT_PIN = A1;
const unsigned long MQ7_READ_INTERVAL_MS = 1000;

int activeMq7Pin = -1;
const char *activeMq7Name = "desligado";

unsigned long lastMq7ReadAt = 0;
int lastMq7RawValue = 0;
bool mq7HasReading = false;
bool mq7ReadingOk = false;
bool mq7ReadingUpdated = false;

void resetMq7ReadingState();

void setupMq7Sensor() {
  pinMode(MQ7_LEFT_PIN, INPUT);
  pinMode(MQ7_RIGHT_PIN, INPUT);
}

void updateMq7Sensor() {
  mq7ReadingUpdated = false;

  if (activeMq7Pin < 0) {
    return;
  }

  unsigned long now = millis();
  if (mq7HasReading && now - lastMq7ReadAt < MQ7_READ_INTERVAL_MS) {
    return;
  }

  lastMq7ReadAt = now;
  lastMq7RawValue = analogRead(activeMq7Pin);
  mq7HasReading = true;
  mq7ReadingOk = true;
  mq7ReadingUpdated = true;
}

void printMq7ReadingToSerial() {
  if (!mq7ReadingUpdated) {
    return;
  }

  Serial.print("MQ7 ");
  Serial.print(activeMq7Name);
  Serial.print(": raw=");
  Serial.println(lastMq7RawValue);
}

void selectMq7LeftArm() {
  activeMq7Pin = MQ7_LEFT_PIN;
  activeMq7Name = "braco esquerdo A0";
  resetMq7ReadingState();
  Serial.println("MQ7: selecionado braco esquerdo A0");
}

void selectMq7RightArm() {
  activeMq7Pin = MQ7_RIGHT_PIN;
  activeMq7Name = "braco direito A1";
  resetMq7ReadingState();
  Serial.println("MQ7: selecionado braco direito A1");
}

void disableMq7Sensor() {
  activeMq7Pin = -1;
  activeMq7Name = "desligado";
  resetMq7ReadingState();
  Serial.println("MQ7: desligado");
}

void resetMq7ReadingState() {
  lastMq7ReadAt = 0;
  lastMq7RawValue = 0;
  mq7HasReading = false;
  mq7ReadingOk = false;
  mq7ReadingUpdated = false;
}

bool hasMq7Reading() {
  return mq7HasReading;
}

bool isMq7ReadingOk() {
  return mq7ReadingOk;
}

int getMq7RawValue() {
  return lastMq7RawValue;
}
