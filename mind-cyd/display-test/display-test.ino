/*
  CYD display test for ESP32-2432S028 / ILI9341.

  Use this sketch if the main mind-cyd screen stays black.
*/

#define USER_SETUP_LOADED
#define ILI9341_DRIVER
#define TFT_WIDTH CYD_TFT_WIDTH
#define TFT_HEIGHT CYD_TFT_HEIGHT
#define TFT_MISO CYD_TFT_MISO
#define TFT_MOSI CYD_TFT_MOSI
#define TFT_SCLK CYD_TFT_SCK
#define TFT_CS CYD_TFT_CS
#define TFT_DC CYD_TFT_DC
#define TFT_RST -1
#define TFT_BL CYD_TFT_BL
#define TFT_BACKLIGHT_ON HIGH
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define USE_HSPI_PORT
#define SPI_FREQUENCY 40000000
#define SPI_READ_FREQUENCY 16000000

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

TFT_eSPI tft;

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

SPIClass touchSpi = SPIClass(VSPI);
XPT2046_Touchscreen touch(XPT2046_CS, XPT2046_IRQ);
bool screenOn = true;
unsigned long lastTouchAt = 0;

void setScreen(bool enabled) {
  screenOn = enabled;
  digitalWrite(TFT_BL, enabled ? TFT_BACKLIGHT_ON : !TFT_BACKLIGHT_ON);
}

void setup() {
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  tft.init();
  tft.setRotation(1);

  touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touch.begin(touchSpi);
  touch.setRotation(1);
}

void loop() {
  tft.fillScreen(TFT_RED);
  delay(700);
  tft.fillScreen(TFT_GREEN);
  delay(700);
  tft.fillScreen(TFT_BLUE);
  delay(700);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("CYD DISPLAY TEST", 20, 40);
  tft.drawString("ILI9341", 20, 70);
  tft.drawString("TOUCH toggles BL", 20, 100);

  unsigned long startedAt = millis();
  while (millis() - startedAt < 3000) {
    if (touch.touched() && millis() - lastTouchAt > 600) {
      lastTouchAt = millis();
      setScreen(!screenOn);
      Serial.println(screenOn ? "Screen ON" : "Screen OFF");
    }
    delay(20);
  }
}
