/*
  CYD backlight scan.

  Upload this if the screen stays completely black. It cycles likely backlight
  pins. Watch the screen for any glow/flicker.
*/

#include <Arduino.h>

const int pins[] = {21, 27, 32, 4, 5, 15, 22};
const int pinCount = sizeof(pins) / sizeof(pins[0]);

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < pinCount; i++) {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
  }
}

void loop() {
  for (int i = 0; i < pinCount; i++) {
    Serial.print("Backlight candidate GPIO ");
    Serial.println(pins[i]);

    for (int j = 0; j < pinCount; j++) {
      digitalWrite(pins[j], LOW);
    }

    digitalWrite(pins[i], HIGH);
    delay(1500);
    digitalWrite(pins[i], LOW);
    delay(300);
  }
}
