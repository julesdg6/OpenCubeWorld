/*
 * display.h - Self-contained, zero-dependency 4-wire SPI ST7789 240x240 IPS driver.
 *
 * Works cleanly on Waveshare ESP32-S3-LCD-1.54 (ESP-IDF 5.x / 6.x, 80 MHz SPI DMA)
 * and AVR fallback with zero external graphics library dependencies.
 */
#pragma once
#include <Arduino.h>
#include <SPI.h>
#include "config.h"

class OcwDisplay {
 public:
  OcwDisplay(int8_t dc, int8_t cs, int8_t sck, int8_t mosi, int8_t rst)
      : dc_(dc), cs_(cs), sck_(sck), mosi_(mosi), rst_(rst) {}

  bool begin(uint32_t spiHz = 80000000UL) {
    spiHz_ = spiHz;
    pinMode(dc_, OUTPUT);
    pinMode(cs_, OUTPUT);
    digitalWrite(cs_, HIGH);
    digitalWrite(dc_, HIGH);

    if (rst_ >= 0) {
      pinMode(rst_, OUTPUT);
      digitalWrite(rst_, HIGH);
      delay(10);
      digitalWrite(rst_, LOW);
      delay(20);
      digitalWrite(rst_, HIGH);
      delay(120);
    }

#if defined(ESP32)
    spi_ = &SPI;
    spi_->begin(sck_, -1, mosi_, cs_);
#else
    spi_ = &SPI;
    spi_->begin();
#endif

    startWrite();
    writeCmd(0x01);  // SWRESET
    endWrite();
    delay(150);

    startWrite();
    writeCmd(0x11);  // SLPOUT
    endWrite();
    delay(120);

    startWrite();
    writeCmd(0x3A);  // COLMOD: 16-bit RGB565
    writeData(0x55);

    writeCmd(0x36);  // MADCTL: rotation 0, RGB order
    writeData(0x00);

    writeCmd(0xB2);  // Porch control
    writeData(0x0C); writeData(0x0C); writeData(0x00); writeData(0x33); writeData(0x33);

    writeCmd(0xB7);  // Gate control
    writeData(0x35);

    writeCmd(0xBB);  // VCOM
    writeData(0x19);

    writeCmd(0xC0);  // LCM control
    writeData(0x2C);

    writeCmd(0xC2);  // VDV and VRH enable
    writeData(0x01);

    writeCmd(0xC3);  // VRH set
    writeData(0x12);

    writeCmd(0xC4);  // VDV set
    writeData(0x20);

    writeCmd(0xC6);  // Frame rate control (60Hz)
    writeData(0x0F);

    writeCmd(0xD0);  // Power control
    writeData(0xA4); writeData(0xA1);

    writeCmd(0x21);  // INVON (required for 1.54" IPS ST7789 panel)
    writeCmd(0x13);  // NORON
    endWrite();
    delay(10);

    startWrite();
    writeCmd(0x29);  // DISPON
    endWrite();
    delay(20);

    fillScreen(0x0000);
    return true;
  }

  void setRotation(uint8_t r) {
    uint8_t madctl = 0x00;
    switch (r & 3) {
      case 0: madctl = 0x00; break;
      case 1: madctl = 0x60; break;
      case 2: madctl = 0xC0; break;
      case 3: madctl = 0xA0; break;
    }
    startWrite();
    writeCmd(0x36);
    writeData(madctl);
    endWrite();
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t col) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > OCW_SCREEN_W) w = OCW_SCREEN_W - x;
    if (y + h > OCW_SCREEN_H) h = OCW_SCREEN_H - y;
    if (w <= 0 || h <= 0) return;

    uint16_t be = (uint16_t)((col >> 8) | (col << 8));
    uint16_t line[OCW_SCREEN_W];
    for (int i = 0; i < w; i++) line[i] = be;

    startWrite();
    setAddrWindow(x, y, x + w - 1, y + h - 1);
    for (int row = 0; row < h; row++) {
#if defined(ESP32)
      spi_->writeBytes((const uint8_t *)line, (uint32_t)w * 2);
#else
      for (int i = 0; i < w; i++) spi_->transfer16(col);
#endif
    }
    endWrite();
  }

  void fillScreen(uint16_t col) {
    fillRect(0, 0, OCW_SCREEN_W, OCW_SCREEN_H, col);
  }

  // Fast single-burst 240x240 RGB565 framebuffer flush (swaps to ST7789 big-endian)
  void draw16bitRGBBitmap(int16_t x, int16_t y, const uint16_t *bitmap, int16_t w, int16_t h) {
    if (!bitmap || w <= 0 || h <= 0) return;
    startWrite();
    setAddrWindow(x, y, x + w - 1, y + h - 1);
#if defined(ESP32)
    // Stream in 24-row big-endian chunks to keep stack small and SPI DMA saturated
    static uint16_t chunk[OCW_SCREEN_W * 24];
    int row = 0;
    while (row < h) {
      int nRows = (h - row > 24) ? 24 : (h - row);
      int nPx = nRows * w;
      const uint16_t *src = bitmap + row * w;
      for (int i = 0; i < nPx; i++) {
        uint16_t v = src[i];
        chunk[i] = (uint16_t)((v >> 8) | (v << 8));
      }
      spi_->writeBytes((const uint8_t *)chunk, (uint32_t)nPx * 2);
      row += nRows;
    }
#else
    for (int i = 0; i < w * h; i++) spi_->transfer16(bitmap[i]);
#endif
    endWrite();
  }

  // Minimal text helpers for compatibility with Replay/CubeLink sketches
  void setTextColor(uint16_t c) { textCol_ = c; }
  void setCursor(int16_t x, int16_t y) { curX_ = x; curY_ = y; }
  void print(const char *s) { (void)s; }
  void print(const String &s) { (void)s; }

  int16_t width() const { return OCW_SCREEN_W; }
  int16_t height() const { return OCW_SCREEN_H; }

 private:
  int8_t dc_, cs_, sck_, mosi_, rst_;
  uint32_t spiHz_ = 80000000UL;
  SPIClass *spi_ = &SPI;
  uint16_t textCol_ = 0xFFFF;
  int16_t curX_ = 0, curY_ = 0;

  inline void startWrite() {
    spi_->beginTransaction(SPISettings(spiHz_, MSBFIRST, SPI_MODE0));
    digitalWrite(cs_, LOW);
  }
  inline void endWrite() {
    digitalWrite(cs_, HIGH);
    spi_->endTransaction();
  }
  inline void writeCmd(uint8_t c) {
    digitalWrite(dc_, LOW);
    spi_->transfer(c);
    digitalWrite(dc_, HIGH);
  }
  inline void writeData(uint8_t d) {
    spi_->transfer(d);
  }
  inline void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    writeCmd(0x2A);  // CASET
    writeData(x0 >> 8); writeData(x0 & 0xFF);
    writeData(x1 >> 8); writeData(x1 & 0xFF);
    writeCmd(0x2B);  // RASET
    writeData(y0 >> 8); writeData(y0 & 0xFF);
    writeData(y1 >> 8); writeData(y1 & 0xFF);
    writeCmd(0x2C);  // RAMWR
  }
};

using Arduino_GFX = OcwDisplay;

inline OcwDisplay *ocwCreateDisplay() {
  return new OcwDisplay(OCW_LCD_DC_PIN, OCW_LCD_CS_PIN,
                        OCW_LCD_SCLK_PIN, OCW_LCD_MOSI_PIN, OCW_LCD_RST_PIN);
}

inline void ocwBacklightOn() {
  pinMode(OCW_LCD_BL_PIN, OUTPUT);
  digitalWrite(OCW_LCD_BL_PIN, HIGH);
}
