/*
 * lcd32.h - Authentic 32x32 1-bit monochrome dot-matrix STN LCD canvas
 * and flicker-free 240x240 RGB565 framebuffer renderer.
 *
 * Original Radica Cube World toys use a 32x32 monochrome LCD (no color,
 * no greyscale, dark near-black ON pixels on pale desaturated STN glass,
 * with subtle passive-matrix persistence when pixels turn off).
 */
#pragma once
#include <Arduino.h>
#include "sprites.h"
#include "display.h"

namespace ocw {

enum LcdPalette {
  LCD_PAL_STN = 0,  // Classic Radica olive-grey STN monochrome LCD
  LCD_PAL_BW  = 1,  // High-contrast pure black on white
  LCD_PAL_INV = 2,  // Inverted high-contrast white on black
  LCD_N_PALS  = 3
};

struct LcdTheme {
  const char *name;
  uint16_t onCol;     // RGB565 dark pixel ON
  uint16_t offCol;    // RGB565 pale glass OFF
  uint16_t gridCol;   // RGB565 subtle dot-matrix pixel gap
  uint16_t bezelOut;  // Outer cube bezel
  uint16_t bezelIn;   // Recessed inner shadow well
};

// A single 32x32 logical 1-bit monochrome LCD with STN persistence tracking.
class Lcd32 {
 public:
  Lcd32();

  void clear(bool on = false);
  void setPx(int x, int y, bool on = true);
  bool getPx(int x, int y) const;
  void hline(int x0, int x1, int y, bool on = true);
  void vline(int x, int y0, int y1, bool on = true);
  void fillRect(int x, int y, int w, int h, bool on = true);
  void invertAll();

  // Blit a 1-bit PROGMEM sprite onto the 32x32 LCD.
  void blit(const OcwSprite1B &sp, int x, int y, bool flipX = false, bool ink = true);

  // Draw crisp 3x5 monochrome pixel text onto the 32x32 LCD.
  void drawText(const char *s, int x, int y, bool ink = true);

  // Authentic Radica effects on the 32x32 grid:
  void drawMazeBoot(uint8_t progress0to32);   // iconic power-on circuit wipe
  void drawVenetianBlind(uint8_t rowsClosed); // lowered blind when vacated
  void drawLcdTestPattern(uint8_t pattern);   // 0=h-stripes, 1=v-stripes, 2=checker, 3=all-on

  // Advance passive-matrix STN persistence buffer (0=off .. 255=full on).
  void stepPersistence(bool fast = false);

  uint8_t pixelLevel(int x, int y) const { return persist_[y][x]; }

 private:
  uint32_t rows_[32];
  uint8_t persist_[32][32];
};

// Flicker-free 240x240 RGB565 compositor (uses PSRAM/SRAM on ESP32, direct fallback on AVR).
class LcdCompositor {
 public:
  explicit LcdCompositor(Arduino_GFX *gfx);
  ~LcdCompositor();

  bool begin();
  void setPalette(LcdPalette p) { pal_ = (LcdPalette)(p % LCD_N_PALS); }
  LcdPalette palette() const { return pal_; }
  const LcdTheme &theme() const;

  void clearFrame(uint16_t col);
  void fillRect(int x, int y, int w, int h, uint16_t col);
  void drawTextScaled(const char *s, int x, int y, int scale, uint16_t col);

  // Render a 32x32 monochrome LCD at (ox, oy) with integer cell scale (e.g. 7 => 224x224).
  void renderLcd32(const Lcd32 &lcd, int ox, int oy, int scale, bool withGrid = true);

  // Draw a recessed cube bezel around an LCD rect.
  void drawRecessedBezel(int ox, int oy, int lcdW, int lcdH, int border);

  // Flush the 240x240 RGB565 framebuffer to the ST7789 in one SPI DMA burst.
  void present();

  static uint16_t blend565(uint16_t offCol, uint16_t onCol, uint8_t alpha);

 private:
  Arduino_GFX *gfx_ = nullptr;
  uint16_t *fb_ = nullptr;
  LcdPalette pal_ = LCD_PAL_STN;
};

}  // namespace ocw
