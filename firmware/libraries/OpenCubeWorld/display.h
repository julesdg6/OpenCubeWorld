/*
 * display.h - ST7789S display factory for the Waveshare 1.54inch Touch
 * LCD Module (SKU 34370).
 *
 * Uses Arduino_GFX (the Waveshare "GFX_Library_for_Arduino" is a fork of
 * it; either installs fine). Pin map: see config.h.
 */
#pragma once
#include <Arduino_GFX_Library.h>
#include "config.h"

inline Arduino_GFX *ocwCreateDisplay() {
  SPI_DataBus *bus = new SPI_DataBus(
      OCW_LCD_CS_PIN, OCW_LCD_DC_PIN, OCW_LCD_SCLK_PIN, OCW_LCD_MOSI_PIN);
  return new Arduino_ST7789(bus, OCW_LCD_RST_PIN, 0 /* rotation */,
                            true /* IPS */, OCW_SCREEN_W, OCW_SCREEN_H);
}

inline void ocwBacklightOn() {
  pinMode(OCW_LCD_BL_PIN, OUTPUT);
  digitalWrite(OCW_LCD_BL_PIN, HIGH);
}
