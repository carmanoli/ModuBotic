/*
  Print CYD board and display diagnostics to Serial Monitor.

  Arduino IDE:
    Tools -> Board -> ESP32-2432S028T CYD
    Tools -> Serial Monitor -> 115200 baud
*/

#include <Arduino.h>

void setup() {
  // First signal: if this blinks, the uploaded sketch is running even if
  // Serial Monitor or display are not working.
#ifdef CYD_LED_RED
  pinMode(CYD_LED_RED, OUTPUT);
  pinMode(CYD_LED_GREEN, OUTPUT);
  pinMode(CYD_LED_BLUE, OUTPUT);
  CYD_LED_RGB_OFF();
  for (int i = 0; i < 6; i++) {
    CYD_LED_BLUE_ON();
    delay(150);
    CYD_LED_BLUE_OFF();
    delay(150);
  }
#endif

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=== CYD board diagnostics ===");

#ifdef ARDUINO_BOARD
  Serial.print("ARDUINO_BOARD: ");
  Serial.println(ARDUINO_BOARD);
#endif

#ifdef ARDUINO_VARIANT
  Serial.print("ARDUINO_VARIANT: ");
  Serial.println(ARDUINO_VARIANT);
#endif

#ifdef ARDUINO_ESP32_2432S028R
  Serial.println("Board macro: ARDUINO_ESP32_2432S028R");
#endif

#ifdef CYD_TFT_CS
  Serial.println("CYD macros detected.");
  Serial.print("CYD_TFT_CS: ");
  Serial.println(CYD_TFT_CS);
  Serial.print("CYD_TFT_DC: ");
  Serial.println(CYD_TFT_DC);
  Serial.print("CYD_TFT_SCK: ");
  Serial.println(CYD_TFT_SCK);
  Serial.print("CYD_TFT_MOSI: ");
  Serial.println(CYD_TFT_MOSI);
  Serial.print("CYD_TFT_MISO: ");
  Serial.println(CYD_TFT_MISO);
  Serial.print("CYD_TFT_BL: ");
  Serial.println(CYD_TFT_BL);
  Serial.print("CYD_TFT_WIDTH: ");
  Serial.println(CYD_TFT_WIDTH);
  Serial.print("CYD_TFT_HEIGHT: ");
  Serial.println(CYD_TFT_HEIGHT);
  Serial.print("CYD_TP_CS: ");
  Serial.println(CYD_TP_CS);
  Serial.print("CYD_TP_IRQ: ");
  Serial.println(CYD_TP_IRQ);
#else
  Serial.println("CYD macros NOT detected.");
#endif

  Serial.println();
  Serial.println("Backlight test on CYD_TFT_BL if available.");
#ifdef CYD_TFT_BL
  CYD_TFT_BL_ENABLE();
  for (int i = 0; i < 5; i++) {
    Serial.println("BL ON");
    CYD_TFT_BL_ON();
    delay(700);
    Serial.println("BL OFF");
    CYD_TFT_BL_OFF();
    delay(700);
  }
  Serial.println("BL ON final");
  CYD_TFT_BL_ON();
#endif

  Serial.println("=== diagnostics done ===");
}

void loop() {
#ifdef CYD_LED_RED
  CYD_LED_GREEN_ON();
  delay(80);
  CYD_LED_GREEN_OFF();
  delay(1920);
#endif
}
