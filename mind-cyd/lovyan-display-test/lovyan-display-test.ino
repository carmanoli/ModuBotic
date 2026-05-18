/*
  CYD display test using LovyanGFX.

  Install in Arduino IDE:
    Tools -> Manage Libraries -> LovyanGFX

  This is useful for USB-C CYD variants that stay black with TFT_eSPI.
*/

#include <Arduino.h>
#include <LovyanGFX.hpp>

class CYDDisplay : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 panel;
  lgfx::Bus_SPI bus;
  lgfx::Light_PWM light;

public:
  CYDDisplay() {
    {
      auto cfg = bus.config();
      cfg.spi_host = HSPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = 1;
      cfg.pin_sclk = CYD_TFT_SCK;
      cfg.pin_mosi = CYD_TFT_MOSI;
      cfg.pin_miso = CYD_TFT_MISO;
      cfg.pin_dc = CYD_TFT_DC;
      bus.config(cfg);
      panel.setBus(&bus);
    }

    {
      auto cfg = panel.config();
      cfg.pin_cs = CYD_TFT_CS;
      cfg.pin_rst = -1;
      cfg.pin_busy = -1;
      cfg.memory_width = CYD_TFT_HEIGHT;
      cfg.memory_height = CYD_TFT_WIDTH;
      cfg.panel_width = CYD_TFT_HEIGHT;
      cfg.panel_height = CYD_TFT_WIDTH;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = true;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      panel.config(cfg);
    }

    {
      auto cfg = light.config();
      cfg.pin_bl = CYD_TFT_BL;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      light.config(cfg);
      panel.setLight(&light);
    }

    setPanel(&panel);
  }
};

CYDDisplay display;

void setup() {
  Serial.begin(115200);

  CYD_TFT_BL_ENABLE();
  CYD_TFT_BL_ON();

  display.init();
  display.setRotation(1);
  display.setBrightness(255);
}

void loop() {
  display.fillScreen(TFT_RED);
  delay(700);
  display.fillScreen(TFT_GREEN);
  delay(700);
  display.fillScreen(TFT_BLUE);
  delay(700);
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.setCursor(16, 32);
  display.println("LovyanGFX CYD");
  display.println("ILI9341 test");
  display.println("BL 21/27 HIGH");
  delay(1800);
}
