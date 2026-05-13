#include <Arduino.h>
#include <string.h>
#include "keypad_menu.h"
#include "lcd_display.h"

const int KEYPAD_ANALOG_PIN = A3;
const unsigned long KEYPAD_DEBOUNCE_MS = 80;
const unsigned long KEYPAD_REPEAT_MS = 350;
const unsigned long KEYPAD_DIM_AFTER_MS = 5000;
const unsigned long KEYPAD_SLEEP_AFTER_MS = 10000;

enum KeypadKey {
  KEYPAD_NONE,
  KEYPAD_RIGHT,
  KEYPAD_UP,
  KEYPAD_DOWN,
  KEYPAD_LEFT,
  KEYPAD_SELECT
};

struct MenuCommand {
  const char *label;
  const char *command;
};

const MenuCommand MENU_COMMANDS[] = {
  {"Parar", "STOP"},
  {"Frente", "MOVE_FORWARD"},
  {"Tras", "MOVE_BACK"},
  {"Virar esq.", "TURN_LEFT"},
  {"Virar dir.", "TURN_RIGHT"},
  {"Cabeca esq.", "HEAD_LEFT"},
  {"Cabeca meio", "HEAD_CENTER"},
  {"Cabeca dir.", "HEAD_RIGHT"},
  {"Braco esq. cima", "ARM_LEFT_UP"},
  {"Braco esq. baixo", "ARM_LEFT_DOWN"},
  {"Braco dir. cima", "ARM_RIGHT_UP"},
  {"Braco dir. baixo", "ARM_RIGHT_DOWN"},
  {"Olhos on", "EYES_ON"},
  {"Olhos off", "EYES_OFF"},
  {"Ambiente", "ENV_READ"},
  {"Cor", "GY33_READ"},
  {"LCD off", "LCD_OFF"}
};

const byte MENU_COMMAND_COUNT = sizeof(MENU_COMMANDS) / sizeof(MENU_COMMANDS[0]);

byte selectedMenuIndex = 0;
KeypadKey lastRawKey = KEYPAD_NONE;
KeypadKey debouncedKey = KEYPAD_NONE;
KeypadKey lastHandledKey = KEYPAD_NONE;
unsigned long rawKeyChangedAt = 0;
unsigned long lastHandledAt = 0;
unsigned long lastActivityAt = 0;
bool keypadDisplayAwake = false;
bool keypadDisplayDimmed = false;
bool menuNeedsRedraw = true;

KeypadKey readKeypadKey();
void handleKeypadKey(KeypadKey key, KeypadMenuCommandHandler commandHandler);
void moveMenuSelection(int delta);
void executeMenuCommand(KeypadMenuCommandHandler commandHandler);
void wakeKeypadDisplay();
void updateKeypadDisplayPower();
void drawKeypadMenu();
void formatMenuLine(char *buffer, size_t size, const char *prefix, const char *text);

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

  updateKeypadDisplayPower();

  if (keypadDisplayAwake && menuNeedsRedraw) {
    drawKeypadMenu();
    menuNeedsRedraw = false;
  }
}

KeypadKey readKeypadKey() {
  int value = analogRead(KEYPAD_ANALOG_PIN);

  if (value < 70) {
    return KEYPAD_RIGHT;
  }
  if (value < 230) {
    return KEYPAD_UP;
  }
  if (value < 420) {
    return KEYPAD_DOWN;
  }
  if (value < 620) {
    return KEYPAD_LEFT;
  }
  if (value < 900) {
    return KEYPAD_SELECT;
  }

  return KEYPAD_NONE;
}

void handleKeypadKey(KeypadKey key, KeypadMenuCommandHandler commandHandler) {
  wakeKeypadDisplay();

  if (key == KEYPAD_UP || key == KEYPAD_LEFT) {
    moveMenuSelection(-1);
  } else if (key == KEYPAD_DOWN || key == KEYPAD_RIGHT) {
    moveMenuSelection(1);
  } else if (key == KEYPAD_SELECT) {
    executeMenuCommand(commandHandler);
  }
}

void moveMenuSelection(int delta) {
  int nextIndex = selectedMenuIndex + delta;

  if (nextIndex < 0) {
    nextIndex = MENU_COMMAND_COUNT - 1;
  } else if (nextIndex >= MENU_COMMAND_COUNT) {
    nextIndex = 0;
  }

  selectedMenuIndex = nextIndex;
  menuNeedsRedraw = true;
}

void executeMenuCommand(KeypadMenuCommandHandler commandHandler) {
  char line1[17];
  char line2[17];

  formatMenuLine(line1, sizeof(line1), "Executar", MENU_COMMANDS[selectedMenuIndex].label);
  formatMenuLine(line2, sizeof(line2), "", MENU_COMMANDS[selectedMenuIndex].command);
  showLcdMessage(line1, line2);

  if (commandHandler != NULL) {
    commandHandler(String(MENU_COMMANDS[selectedMenuIndex].command), "KEYPAD");
  }

  menuNeedsRedraw = false;
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
  char line1[17];
  char line2[17];
  char position[6];

  snprintf(position, sizeof(position), "%02u/%02u", selectedMenuIndex + 1, MENU_COMMAND_COUNT);
  formatMenuLine(line1, sizeof(line1), "Menu", position);
  formatMenuLine(line2, sizeof(line2), ">", MENU_COMMANDS[selectedMenuIndex].label);
  showLcdMessage(line1, line2);
}

void formatMenuLine(char *buffer, size_t size, const char *prefix, const char *text) {
  if (prefix == NULL || prefix[0] == '\0') {
    snprintf(buffer, size, "%s", text);
  } else {
    snprintf(buffer, size, "%s %s", prefix, text);
  }
}
