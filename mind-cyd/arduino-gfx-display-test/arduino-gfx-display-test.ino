/*
  CYD display test using Arduino_GFX.

  Install in Arduino IDE:
    Tools -> Manage Libraries -> GFX Library for Arduino
*/

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

Arduino_DataBus *bus = new Arduino_ESP32SPI(
  CYD_TFT_DC,
  CYD_TFT_CS,
  CYD_TFT_SCK,
  CYD_TFT_MOSI,
  CYD_TFT_MISO,
  CYD_TFT_SPI_BUS
);

Arduino_GFX *gfx = new Arduino_ILI9341(
  bus,
  -1,     // RST
  1,      // rotation
  false,  // IPS
  CYD_TFT_HEIGHT,
  CYD_TFT_WIDTH
);

void setup() {
  Serial.begin(115200);
  CYD_TFT_BL_ENABLE();
  CYD_TFT_BL_ON();

  if (!gfx->begin()) {
    Serial.println("gfx begin failed");
  }

  gfx->fillScreen(RED);
  delay(600);
  gfx->fillScreen(GREEN);
  delay(600);
  gfx->fillScreen(BLUE);
  delay(600);
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(20, 40);
  gfx->println("Arduino_GFX");
  gfx->println("ESP32-2432S028");
}

void loop() {
}
