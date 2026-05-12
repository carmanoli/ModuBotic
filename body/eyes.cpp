#include <Arduino.h>
#include "eyes.h"

const int EYES_DIN_PIN = 4;
const int EYES_CS_PIN = 5;
const int EYES_CLK_PIN = 6;
const int EYES_MATRIX_COUNT = 2;

const unsigned long EYES_OPEN_MS = 3000;
const unsigned long EYES_HALF_MS = 80;
const unsigned long EYES_CLOSED_MS = 140;
const unsigned long EYES_BOOT_TEST_MS = 350;
const unsigned long EYES_LED_SCAN_MS = 80;
const bool EYES_SINGLE_LED_TEST = false;
const bool EYES_LED_SCAN_TEST = false;

const byte EYE_OPEN[8][8] = {
  {0, 0, 1, 1, 1, 1, 0, 0},
  {0, 1, 1, 1, 1, 1, 1, 0},
  {1, 1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 0, 0, 1, 1, 1},
  {1, 1, 1, 0, 0, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1, 1},
  {0, 1, 1, 1, 1, 1, 1, 0},
  {0, 0, 1, 1, 1, 1, 0, 0}
};

const byte EYE_HALF[8][8] = {
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 1, 1, 1, 1, 0, 0},
  {0, 1, 1, 1, 1, 1, 1, 0},
  {0, 1, 1, 1, 1, 1, 1, 0},
  {0, 0, 1, 1, 1, 1, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0}
};

const byte EYE_CLOSED[8][8] = {
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {1, 1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1, 1},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0}
};

unsigned long lastEyeFrameAt = 0;
unsigned long lastEyeScanAt = 0;
int eyeBlinkState = 0;
int eyeScanLed = 0;
bool eyesEnabled = true;
bool eyesRuntimeScanTest = false;

void clearEyes();
void bootTestEyes();
void drawEyes(const byte frame[8][8]);
byte eyeRowToByte(const byte row[8]);
void eyesDrawMatrix(int targetMatrix, byte value);
void drawSingleEyeLed();
void updateEyeLedScan();
void eyesWriteAll(byte address, byte value);
void eyesWriteOne(int targetMatrix, byte address, byte value);

void setupEyes() {
  pinMode(EYES_DIN_PIN, OUTPUT);
  pinMode(EYES_CS_PIN, OUTPUT);
  pinMode(EYES_CLK_PIN, OUTPUT);

  digitalWrite(EYES_CS_PIN, HIGH);
  digitalWrite(EYES_DIN_PIN, LOW);
  digitalWrite(EYES_CLK_PIN, LOW);

  eyesWriteAll(0x0C, 0x00);  // shutdown while configuring
  eyesWriteAll(0x09, 0x00);  // no decode mode
  eyesWriteAll(0x0A, 0x02);  // low brightness, 0x00-0x0F
  eyesWriteAll(0x0B, 0x07);  // scan all 8 rows
  eyesWriteAll(0x0F, 0x00);  // display test off
  clearEyes();
  eyesWriteAll(0x0C, 0x01);  // normal operation

  clearEyes();
  if (EYES_SINGLE_LED_TEST || EYES_LED_SCAN_TEST) {
    drawSingleEyeLed();
  } else {
    bootTestEyes();
    clearEyes();
    drawEyes(EYE_OPEN);
  }
  lastEyeFrameAt = millis();
}

void updateEyes() {
  if (!eyesEnabled) {
    return;
  }

  if (EYES_SINGLE_LED_TEST) {
    return;
  }

  if (EYES_LED_SCAN_TEST || eyesRuntimeScanTest) {
    updateEyeLedScan();
    return;
  }

  unsigned long now = millis();
  unsigned long interval = EYES_OPEN_MS;

  if (eyeBlinkState == 1 || eyeBlinkState == 3) {
    interval = EYES_HALF_MS;
  } else if (eyeBlinkState == 2) {
    interval = EYES_CLOSED_MS;
  }

  if (now - lastEyeFrameAt < interval) {
    return;
  }

  lastEyeFrameAt = now;
  eyeBlinkState = (eyeBlinkState + 1) % 4;

  if (eyeBlinkState == 0) {
    drawEyes(EYE_OPEN);
  } else if (eyeBlinkState == 1 || eyeBlinkState == 3) {
    drawEyes(EYE_HALF);
  } else {
    drawEyes(EYE_CLOSED);
  }
}

void resetEyes() {
  eyesEnabled = true;
  eyesRuntimeScanTest = false;
  setupEyes();
}

void enableEyes(bool enabled) {
  eyesEnabled = enabled;
  if (eyesEnabled) {
    eyesRuntimeScanTest = false;
    setupEyes();
  } else {
    clearEyes();
  }
}

bool isEyesEnabled() {
  return eyesEnabled;
}

void showEyesAllOn() {
  eyesEnabled = false;
  eyesRuntimeScanTest = false;
  for (byte row = 1; row <= 8; row++) {
    eyesWriteAll(row, 0xFF);
  }
}

void clearEyesForTest() {
  eyesEnabled = false;
  eyesRuntimeScanTest = false;
  clearEyes();
}

void startEyesScanTest() {
  eyesEnabled = true;
  eyesRuntimeScanTest = true;
  eyeScanLed = 0;
  lastEyeScanAt = 0;
  clearEyes();
}

void clearEyes() {
  for (byte row = 1; row <= 8; row++) {
    eyesWriteAll(row, 0x00);
  }
}

void bootTestEyes() {
  for (int matrix = 0; matrix < EYES_MATRIX_COUNT; matrix++) {
    eyesDrawMatrix(matrix, 0xFF);
    delay(EYES_BOOT_TEST_MS);
    eyesDrawMatrix(matrix, 0x00);
  }
}

void drawEyes(const byte frame[8][8]) {
  for (byte row = 0; row < 8; row++) {
    eyesWriteAll(row + 1, eyeRowToByte(frame[row]));
  }
}

byte eyeRowToByte(const byte row[8]) {
  byte value = 0;
  for (byte column = 0; column < 8; column++) {
    if (row[column] == 1) {
      value |= 1 << (7 - column);
    }
  }
  return value;
}

void eyesDrawMatrix(int targetMatrix, byte value) {
  for (byte row = 1; row <= 8; row++) {
    eyesWriteOne(targetMatrix, row, value);
  }
}

void drawSingleEyeLed() {
  const int matrix = 0;
  const byte row = 4;
  const byte columnBit = B00001000;
  eyesWriteOne(matrix, row, columnBit);
}

void updateEyeLedScan() {
  unsigned long now = millis();
  if (now - lastEyeScanAt < EYES_LED_SCAN_MS) {
    return;
  }

  lastEyeScanAt = now;
  clearEyes();

  int matrix = eyeScanLed / 64;
  int ledInMatrix = eyeScanLed % 64;
  byte row = (ledInMatrix / 8) + 1;
  byte columnBit = 1 << (ledInMatrix % 8);
  eyesWriteOne(matrix, row, columnBit);

  eyeScanLed = (eyeScanLed + 1) % (EYES_MATRIX_COUNT * 64);
}

void eyesWriteAll(byte address, byte value) {
  digitalWrite(EYES_CS_PIN, LOW);
  for (int matrix = 0; matrix < EYES_MATRIX_COUNT; matrix++) {
    shiftOut(EYES_DIN_PIN, EYES_CLK_PIN, MSBFIRST, address);
    shiftOut(EYES_DIN_PIN, EYES_CLK_PIN, MSBFIRST, value);
  }
  digitalWrite(EYES_CS_PIN, HIGH);
}

void eyesWriteOne(int targetMatrix, byte address, byte value) {
  digitalWrite(EYES_CS_PIN, LOW);
  for (int matrix = EYES_MATRIX_COUNT - 1; matrix >= 0; matrix--) {
    if (matrix == targetMatrix) {
      shiftOut(EYES_DIN_PIN, EYES_CLK_PIN, MSBFIRST, address);
      shiftOut(EYES_DIN_PIN, EYES_CLK_PIN, MSBFIRST, value);
    } else {
      shiftOut(EYES_DIN_PIN, EYES_CLK_PIN, MSBFIRST, 0x00);
      shiftOut(EYES_DIN_PIN, EYES_CLK_PIN, MSBFIRST, 0x00);
    }
  }
  digitalWrite(EYES_CS_PIN, HIGH);
}
