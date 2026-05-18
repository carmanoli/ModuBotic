#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "keypad_menu.h"
#include "lcd_display.h"
#include "gy33_color_sensor.h"
#include "aht20_bmp280_sensor.h"
#include "i2c_bus_watchdog.h"

const int KEYPAD_ANALOG_PIN = A3;
const unsigned long KEYPAD_DEBOUNCE_MS = 80;
const unsigned long KEYPAD_REPEAT_MS = 350;
const unsigned long KEYPAD_DIM_AFTER_MS = 5000;
const unsigned long KEYPAD_SLEEP_AFTER_MS = 10000;
const unsigned long SENSOR_SCREEN_REFRESH_MS = 1000;

enum KeypadKey {
  KEYPAD_NONE,
  KEYPAD_RIGHT,
  KEYPAD_UP,
  KEYPAD_DOWN,
  KEYPAD_LEFT,
  KEYPAD_SELECT
};

enum MenuScreen {
  SCREEN_HOME,
  SCREEN_MIND,
  SCREEN_BODY,
  SCREEN_BODY_HEAD,
  SCREEN_BODY_LEFT_ARM,
  SCREEN_BODY_RIGHT_ARM,
  SCREEN_BODY_LEFT_WHEEL,
  SCREEN_BODY_RIGHT_WHEEL,
  SCREEN_SENSOR
};

struct MenuItem {
  const char *label;
  MenuScreen target;
  const char *command;
};

const MenuItem HOME_ITEMS[] = {
  {"Mind", SCREEN_MIND, NULL},
  {"Body", SCREEN_BODY, NULL},
  {"Sensor", SCREEN_SENSOR, NULL}
};

const MenuItem MIND_ITEMS[] = {
  {"IP", SCREEN_MIND, NULL}
};

const MenuItem BODY_ITEMS[] = {
  {"Head", SCREEN_BODY_HEAD, NULL},
  {"Left Arm", SCREEN_BODY_LEFT_ARM, NULL},
  {"Right Arm", SCREEN_BODY_RIGHT_ARM, NULL},
  {"Left Wheel", SCREEN_BODY_LEFT_WHEEL, NULL},
  {"Right Wheel", SCREEN_BODY_RIGHT_WHEEL, NULL}
};

const MenuItem HEAD_ITEMS[] = {
  {"Move Left", SCREEN_BODY_HEAD, "HEAD_LEFT"},
  {"Move Center", SCREEN_BODY_HEAD, "HEAD_CENTER"},
  {"Move Right", SCREEN_BODY_HEAD, "HEAD_RIGHT"}
};

const MenuItem LEFT_ARM_ITEMS[] = {
  {"Move Up", SCREEN_BODY_LEFT_ARM, "ARM_LEFT_UP"},
  {"Move Center", SCREEN_BODY_LEFT_ARM, "ARM_LEFT_CENTER"},
  {"Move Down", SCREEN_BODY_LEFT_ARM, "ARM_LEFT_DOWN"}
};

const MenuItem RIGHT_ARM_ITEMS[] = {
  {"Move Up", SCREEN_BODY_RIGHT_ARM, "ARM_RIGHT_UP"},
  {"Move Center", SCREEN_BODY_RIGHT_ARM, "ARM_RIGHT_CENTER"},
  {"Move Down", SCREEN_BODY_RIGHT_ARM, "ARM_RIGHT_DOWN"}
};

const MenuItem LEFT_WHEEL_ITEMS[] = {
  {"Forward", SCREEN_BODY_LEFT_WHEEL, "LEFT_WHEEL_FORWARD"},
  {"Back", SCREEN_BODY_LEFT_WHEEL, "LEFT_WHEEL_BACK"},
  {"Stop", SCREEN_BODY_LEFT_WHEEL, "STOP"}
};

const MenuItem RIGHT_WHEEL_ITEMS[] = {
  {"Forward", SCREEN_BODY_RIGHT_WHEEL, "RIGHT_WHEEL_FORWARD"},
  {"Back", SCREEN_BODY_RIGHT_WHEEL, "RIGHT_WHEEL_BACK"},
  {"Stop", SCREEN_BODY_RIGHT_WHEEL, "STOP"}
};

MenuScreen activeScreen = SCREEN_HOME;
byte selectedIndex = 0;
KeypadKey lastRawKey = KEYPAD_NONE;
KeypadKey debouncedKey = KEYPAD_NONE;
KeypadKey lastHandledKey = KEYPAD_NONE;
unsigned long rawKeyChangedAt = 0;
unsigned long lastHandledAt = 0;
unsigned long lastActivityAt = 0;
unsigned long lastSensorScreenRefreshAt = 0;
bool keypadDisplayAwake = false;
bool keypadDisplayDimmed = false;
bool menuNeedsRedraw = true;

KeypadKey readKeypadKey();
void handleKeypadKey(KeypadKey key, KeypadMenuCommandHandler commandHandler);
void moveMenuSelection(int delta);
void enterSelectedItem(KeypadMenuCommandHandler commandHandler);
void goBack();
void setActiveScreen(MenuScreen screen);
void executeCommand(const char *label, const char *command, KeypadMenuCommandHandler commandHandler);
void wakeKeypadDisplay();
void updateKeypadDisplayPower();
void drawKeypadMenu();
void drawSensorScreen();
void drawMindScreen();
void printScreenLine(const char *line1, const char *line2);
void formatMenuLine(char *buffer, size_t size, const char *prefix, const char *text);
void formatFloat(char *buffer, size_t size, float value, byte decimals);
void formatI2cSignature(char *buffer, size_t size);
const MenuItem *getCurrentItems();
byte getCurrentItemCount();
const char *getScreenTitle();

void setupKeypadMenu() {
  pinMode(KEYPAD_ANALOG_PIN, INPUT);
  lastActivityAt = millis();
}

void updateKeypadMenu(KeypadMenuCommandHandler commandHandler) {
  KeypadKey rawKey = readKeypadKey();
  unsigned long now = millis();

  if (rawKey != lastRawKey) {
    lastRawKey = rawKey;
    rawKeyChangedAt = now;
  }

  if (now - rawKeyChangedAt >= KEYPAD_DEBOUNCE_MS) {
    debouncedKey = rawKey;
  }

  if (debouncedKey != KEYPAD_NONE) {
    bool firstPress = lastHandledKey == KEYPAD_NONE;
    bool canRepeat = debouncedKey != KEYPAD_SELECT;
    bool repeatedPress = canRepeat && debouncedKey == lastHandledKey && now - lastHandledAt >= KEYPAD_REPEAT_MS;

    if (firstPress || repeatedPress) {
      handleKeypadKey(debouncedKey, commandHandler);
      lastHandledKey = debouncedKey;
      lastHandledAt = now;
    }
  } else {
    lastHandledKey = KEYPAD_NONE;
  }

  if (keypadDisplayAwake && activeScreen == SCREEN_SENSOR && now - lastSensorScreenRefreshAt >= SENSOR_SCREEN_REFRESH_MS) {
    menuNeedsRedraw = true;
  }

  updateKeypadDisplayPower();

  if (keypadDisplayAwake && menuNeedsRedraw) {
    drawKeypadMenu();
    menuNeedsRedraw = false;
  }
}

KeypadKey readKeypadKey() {
  int value = analogRead(KEYPAD_ANALOG_PIN);

  if (value < 70) {
    return KEYPAD_LEFT;
  }
  if (value < 230) {
    return KEYPAD_UP;
  }
  if (value < 420) {
    return KEYPAD_DOWN;
  }
  if (value < 620) {
    return KEYPAD_RIGHT;
  }
  if (value < 900) {
    return KEYPAD_SELECT;
  }

  return KEYPAD_NONE;
}

void handleKeypadKey(KeypadKey key, KeypadMenuCommandHandler commandHandler) {
  wakeKeypadDisplay();

  if (key == KEYPAD_UP) {
    moveMenuSelection(-1);
  } else if (key == KEYPAD_DOWN) {
    moveMenuSelection(1);
  } else if (key == KEYPAD_LEFT) {
    goBack();
  } else if (key == KEYPAD_RIGHT || key == KEYPAD_SELECT) {
    enterSelectedItem(commandHandler);
  }
}

void moveMenuSelection(int delta) {
  byte itemCount = getCurrentItemCount();
  if (itemCount == 0) {
    return;
  }

  int nextIndex = selectedIndex + delta;
  if (nextIndex < 0) {
    nextIndex = itemCount - 1;
  } else if (nextIndex >= itemCount) {
    nextIndex = 0;
  }

  selectedIndex = nextIndex;
  menuNeedsRedraw = true;
}

void enterSelectedItem(KeypadMenuCommandHandler commandHandler) {
  const MenuItem *items = getCurrentItems();
  byte itemCount = getCurrentItemCount();

  if (activeScreen == SCREEN_MIND) {
    drawMindScreen();
    return;
  }

  if (activeScreen == SCREEN_SENSOR) {
    if (selectedIndex == 0) {
      executeCommand("Refresh Color", "GY33_READ", commandHandler);
    } else if (selectedIndex == 1) {
      executeCommand("Refresh Env", "ENV_READ", commandHandler);
    } else if (selectedIndex == 2) {
      executeCommand("Refresh Env", "ENV_READ", commandHandler);
    } else {
      executeCommand("Scan I2C", "I2C_SCAN", commandHandler);
    }
    return;
  }

  if (items == NULL || selectedIndex >= itemCount) {
    return;
  }

  const MenuItem &item = items[selectedIndex];
  if (item.command != NULL) {
    executeCommand(item.label, item.command, commandHandler);
    return;
  }

  setActiveScreen(item.target);
}

void goBack() {
  if (activeScreen == SCREEN_HOME) {
    return;
  }

  if (activeScreen == SCREEN_BODY_HEAD ||
      activeScreen == SCREEN_BODY_LEFT_ARM ||
      activeScreen == SCREEN_BODY_RIGHT_ARM ||
      activeScreen == SCREEN_BODY_LEFT_WHEEL ||
      activeScreen == SCREEN_BODY_RIGHT_WHEEL) {
    setActiveScreen(SCREEN_BODY);
    return;
  }

  setActiveScreen(SCREEN_HOME);
}

void setActiveScreen(MenuScreen screen) {
  activeScreen = screen;
  selectedIndex = 0;
  menuNeedsRedraw = true;
}

void executeCommand(const char *label, const char *command, KeypadMenuCommandHandler commandHandler) {
  char line1[17];
  char line2[17];

  formatMenuLine(line1, sizeof(line1), getScreenTitle(), "OK");
  formatMenuLine(line2, sizeof(line2), ">", label);
  printScreenLine(line1, line2);

  if (commandHandler != NULL) {
    commandHandler(String(command), "KEYPAD");
  }

  menuNeedsRedraw = true;
}

void wakeKeypadDisplay() {
  lastActivityAt = millis();
  keypadDisplayAwake = true;
  keypadDisplayDimmed = false;
  setLcdBacklightFull();
}

void updateKeypadDisplayPower() {
  if (!keypadDisplayAwake) {
    return;
  }

  unsigned long idleMs = millis() - lastActivityAt;

  if (idleMs >= KEYPAD_SLEEP_AFTER_MS) {
    sleepLcdDisplay();
    keypadDisplayAwake = false;
    keypadDisplayDimmed = false;
  } else if (idleMs >= KEYPAD_DIM_AFTER_MS && !keypadDisplayDimmed) {
    setLcdBacklightDim();
    keypadDisplayDimmed = true;
  }
}

void drawKeypadMenu() {
  if (activeScreen == SCREEN_SENSOR) {
    drawSensorScreen();
    return;
  }

  if (activeScreen == SCREEN_MIND) {
    drawMindScreen();
    return;
  }

  const MenuItem *items = getCurrentItems();
  byte itemCount = getCurrentItemCount();
  char line1[17];
  char line2[17];
  char position[6];

  if (items == NULL || itemCount == 0) {
    printScreenLine(getScreenTitle(), "Sem opcoes");
    return;
  }

  snprintf(position, sizeof(position), "%u/%u", selectedIndex + 1, itemCount);
  formatMenuLine(line1, sizeof(line1), getScreenTitle(), position);
  formatMenuLine(line2, sizeof(line2), ">", items[selectedIndex].label);
  printScreenLine(line1, line2);
}

void drawSensorScreen() {
  char line1[17];
  char line2[17];

  if (selectedIndex == 0) {
    snprintf(line1, sizeof(line1), "Sensor RGB");
    snprintf(line2, sizeof(line2), "R%u G%u B%u", getGy33Red8(), getGy33Green8(), getGy33Blue8());
  } else if (selectedIndex == 1) {
    char temp[7];
    char humidity[7];
    formatFloat(temp, sizeof(temp), getAht20TemperatureC(), 1);
    formatFloat(humidity, sizeof(humidity), getAht20Humidity(), 0);
    snprintf(line1, sizeof(line1), "Sensor Env");
    snprintf(line2, sizeof(line2), "T%sC H%s%%", temp, humidity);
  } else {
    if (selectedIndex == 2) {
      char pressure[8];
      formatFloat(pressure, sizeof(pressure), getBmp280PressureHpa(), 0);
      snprintf(line1, sizeof(line1), "Sensor Press");
      snprintf(line2, sizeof(line2), "%s hPa", pressure);
    } else {
      char signature[17];
      formatI2cSignature(signature, sizeof(signature));
      snprintf(line1, sizeof(line1), "I2C Bus");
      snprintf(line2, sizeof(line2), "%s", signature);
    }
  }

  lastSensorScreenRefreshAt = millis();
  printScreenLine(line1, line2);
}

void drawMindScreen() {
  printScreenLine("Mind", "IP no painel");
}

void printScreenLine(const char *line1, const char *line2) {
  showLcdMessage(line1, line2);
}

void formatMenuLine(char *buffer, size_t size, const char *prefix, const char *text) {
  if (prefix == NULL || prefix[0] == '\0') {
    snprintf(buffer, size, "%s", text);
  } else {
    snprintf(buffer, size, "%s %s", prefix, text);
  }
}

void formatFloat(char *buffer, size_t size, float value, byte decimals) {
  if (isnan(value)) {
    snprintf(buffer, size, "--");
    return;
  }

  dtostrf(value, 0, decimals, buffer);
}

void formatI2cSignature(char *buffer, size_t size) {
  const char *signature = getI2cBusSignature();
  if (signature == NULL || signature[0] == '\0') {
    snprintf(buffer, size, "SELECT scan");
    return;
  }

  snprintf(buffer, size, "%s", signature);
}

const MenuItem *getCurrentItems() {
  switch (activeScreen) {
    case SCREEN_HOME:
      return HOME_ITEMS;
    case SCREEN_MIND:
      return MIND_ITEMS;
    case SCREEN_BODY:
      return BODY_ITEMS;
    case SCREEN_BODY_HEAD:
      return HEAD_ITEMS;
    case SCREEN_BODY_LEFT_ARM:
      return LEFT_ARM_ITEMS;
    case SCREEN_BODY_RIGHT_ARM:
      return RIGHT_ARM_ITEMS;
    case SCREEN_BODY_LEFT_WHEEL:
      return LEFT_WHEEL_ITEMS;
    case SCREEN_BODY_RIGHT_WHEEL:
      return RIGHT_WHEEL_ITEMS;
    default:
      return NULL;
  }
}

byte getCurrentItemCount() {
  switch (activeScreen) {
    case SCREEN_HOME:
      return sizeof(HOME_ITEMS) / sizeof(HOME_ITEMS[0]);
    case SCREEN_MIND:
      return sizeof(MIND_ITEMS) / sizeof(MIND_ITEMS[0]);
    case SCREEN_BODY:
      return sizeof(BODY_ITEMS) / sizeof(BODY_ITEMS[0]);
    case SCREEN_BODY_HEAD:
      return sizeof(HEAD_ITEMS) / sizeof(HEAD_ITEMS[0]);
    case SCREEN_BODY_LEFT_ARM:
      return sizeof(LEFT_ARM_ITEMS) / sizeof(LEFT_ARM_ITEMS[0]);
    case SCREEN_BODY_RIGHT_ARM:
      return sizeof(RIGHT_ARM_ITEMS) / sizeof(RIGHT_ARM_ITEMS[0]);
    case SCREEN_BODY_LEFT_WHEEL:
      return sizeof(LEFT_WHEEL_ITEMS) / sizeof(LEFT_WHEEL_ITEMS[0]);
    case SCREEN_BODY_RIGHT_WHEEL:
      return sizeof(RIGHT_WHEEL_ITEMS) / sizeof(RIGHT_WHEEL_ITEMS[0]);
    case SCREEN_SENSOR:
      return 4;
    default:
      return 0;
  }
}

const char *getScreenTitle() {
  switch (activeScreen) {
    case SCREEN_HOME:
      return "Menu";
    case SCREEN_MIND:
      return "Mind";
    case SCREEN_BODY:
      return "Body";
    case SCREEN_BODY_HEAD:
      return "Head";
    case SCREEN_BODY_LEFT_ARM:
      return "Left Arm";
    case SCREEN_BODY_RIGHT_ARM:
      return "Right Arm";
    case SCREEN_BODY_LEFT_WHEEL:
      return "Left Wheel";
    case SCREEN_BODY_RIGHT_WHEEL:
      return "Right Wheel";
    case SCREEN_SENSOR:
      return "Sensor";
    default:
      return "Menu";
  }
}
