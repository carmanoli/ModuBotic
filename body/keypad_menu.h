#ifndef MODUBOTIC_KEYPAD_MENU_H
#define MODUBOTIC_KEYPAD_MENU_H

#include <Arduino.h>

typedef void (*KeypadMenuCommandHandler)(String command, const char *source);

void setupKeypadMenu();
void updateKeypadMenu(KeypadMenuCommandHandler commandHandler);

#endif
