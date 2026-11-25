/*
 * lcd32.cpp - Authentic 32x32 1-bit monochrome LCD canvas & 240x240 compositor.
 */
#include "lcd32.h"
#include <string.h>

#if defined(ESP32)
#include <esp_heap_caps.h>
#endif

namespace ocw {

static uint32_t readRowU32(const uint32_t *p, int i) {
#if defined(__AVR__)
  return pgm_read_dword(p + i);
#else
  return p[i];
#endif
}

static const LcdTheme kThemes[LCD_N_PALS] = {
    // Classic Radica STN monochrome olive-grey LCD
    {"STN", 0x18E2, 0xBE55, 0xB5D3, 0x2104, 0x0841},
    // Crisp high-contrast pure Black & White
    {"B&W", 0x0000, 0xF7BE, 0xE71C, 0x2104, 0x0841},
    // Inverted crisp White on Black
    {"INV", 0xF7BE, 0x0841, 0x18C3, 0x2104, 0x0000},
};

// ------------------------------------------------------------------ Lcd32

Lcd32::Lcd32() {
  memset(rows_, 0, sizeof(rows_));
  memset(persist_, 0, sizeof(persist_));
}

void Lcd32::clear(bool on) {
  uint32_t v = on ? 0xFFFFFFFFUL : 0UL;
  for (int y = 0; y < 32; y++) rows_[y] = v;
}

void Lcd32::setPx(int x, int y, bool on) {
  if (x < 0 || x >= 32 || y < 0 || y >= 32) return;
  uint32_t mask = 1UL << (31 - x);
  if (on) rows_[y] |= mask;
  else rows_[y] &= ~mask;
}

bool Lcd32::getPx(int x, int y) const {
  if (x < 0 || x >= 32 || y < 0 || y >= 32) return false;
  return (rows_[y] & (1UL << (31 - x))) != 0;
}

void Lcd32::hline(int x0, int x1, int y, bool on) {
  if (y < 0 || y >= 32) return;
  if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
  for (int x = x0; x <= x1; x++) setPx(x, y, on);
}

void Lcd32::vline(int x, int y0, int y1, bool on) {
  if (x < 0 || x >= 32) return;
  if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
  for (int y = y0; y <= y1; y++) setPx(x, y, on);
}

void Lcd32::fillRect(int x, int y, int w, int h, bool on) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
      setPx(x + i, y + j, on);
}

void Lcd32::invertAll() {
  for (int y = 0; y < 32; y++) rows_[y] = ~rows_[y];
}

void Lcd32::blit(const OcwSprite1B &sp, int x, int y, bool flipX, bool ink) {
  for (int sy = 0; sy < sp.h; sy++) {
    uint32_t row = readRowU32(sp.rows, sy);
    for (int sx = 0; sx < sp.w; sx++) {
      bool bit = (row & (1UL << (sp.w - 1 - sx))) != 0;
      if (bit) {
        int dx = flipX ? (x + sp.w - 1 - sx) : (x + sx);
        setPx(dx, y + sy, ink);
      }
    }
  }
}

void Lcd32::drawText(const char *s, int x, int y, bool ink) {
  int cx = x;
  for (; *s; s++) {
    for (uint8_t r = 0; r < 5; r++) {
      uint8_t bits = ocwFont3x5Row(*s, r);
      for (int c = 0; c < 3; c++) {
        if (bits & (1 << (2 - c))) setPx(cx + c, y + r, ink);
      }
    }
    cx += 4;
  }
}

void Lcd32::drawMazeBoot(uint8_t progress) {
  // Authentic Radica Cube World power-on maze / circuit wipe pattern
  clear(false);
  int limit = constrain((int)progress, 0, 32);
  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 32; x++) {
      int dist = abs(x - 16) + abs(y - 16);
      if (dist > limit) continue;
      bool ring = ((max(abs(x - 16), abs(y - 16)) & 3) == 0);
      bool spoke = (((x ^ y) & 3) == 0) && ((x + y) & 1);
      if (ring || spoke) setPx(x, y, true);
    }
  }
}

void Lcd32::drawVenetianBlind(uint8_t rowsClosed) {
  int n = constrain((int)rowsClosed, 0, 32);
  for (int y = 0; y < n; y++) {
    // Classic alternating slatted blind lines
    bool slat = (y % 3) != 2;
    hline(0, 31, y, slat);
    if (y == n - 1) hline(0, 31, y, true);
  }
  // Draw little center pull-cord knob below the blind
  if (n > 2 && n < 29) {
    setPx(15, n, true);
    setPx(16, n, true);
    setPx(15, n + 1, true);
    setPx(16, n + 1, true);
  }
}

void Lcd32::drawLcdTestPattern(uint8_t pattern) {
  clear(false);
  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 32; x++) {
      bool on = false;
      switch (pattern & 3) {
        case 0: on = (y & 1) == 0; break;            // horizontal stripes
        case 1: on = (x & 1) == 0; break;            // vertical stripes
        case 2: on = ((x + y) & 1) == 0; break;      // checkerboard
        default: on = true; break;                   // all-on
      }
      setPx(x, y, on);
    }
  }
}

void Lcd32::stepPersistence(bool fast) {
  // Passive-matrix STN LCD: pixels turn ON quickly and decay slightly slower
  const uint8_t rise = fast ? 255 : 190;
  const uint8_t fall = fast ? 255 : 85;
  for (int y = 0; y < 32; y++) {
    uint32_t row = rows_[y];
    for (int x = 0; x < 32; x++) {
      bool on = (row & (1UL << (31 - x))) != 0;
      uint8_t &p = persist_[y][x];
      if (on) {
        p = (p + rise > 255) ? 255 : (uint8_t)(p + rise);
      } else {
        p = (p > fall) ? (uint8_t)(p - fall) : 0;
      }
    }
  }
}

// ------------------------------------------------------------ LcdCompositor

LcdCompositor::LcdCompositor(Arduino_GFX *gfx) : gfx_(gfx) {}

LcdCompositor::~LcdCompositor() {
  if (fb_) {
    free(fb_);
    fb_ = nullptr;
  }
}

bool LcdCompositor::begin() {
#if defined(ESP32)
  if (!fb_) {
    // 240*240*2 = 115,200 bytes: try fast internal SRAM first, then OPI PSRAM
    fb_ = (uint16_t *)heap_caps_malloc(240 * 240 * sizeof(uint16_t),
                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!fb_) {
      fb_ = (uint16_t *)heap_caps_malloc(240 * 240 * sizeof(uint16_t),
                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
  }
#endif
  return fb_ != nullptr;
}

const LcdTheme &LcdCompositor::theme() const {
  return kThemes[pal_];
}

uint16_t LcdCompositor::blend565(uint16_t offCol, uint16_t onCol, uint8_t alpha) {
  if (alpha == 0) return offCol;
  if (alpha == 255) return onCol;
  uint16_t inv = 255 - alpha;
  uint16_t r = (((offCol >> 11) & 0x1F) * inv + ((onCol >> 11) & 0x1F) * alpha) >> 8;
  uint16_t g = (((offCol >> 5) & 0x3F) * inv + ((onCol >> 5) & 0x3F) * alpha) >> 8;
  uint16_t b = ((offCol & 0x1F) * inv + (onCol & 0x1F) * alpha) >> 8;
  return (r << 11) | (g << 5) | b;
}

void LcdCompositor::clearFrame(uint16_t col) {
  if (fb_) {
    for (int i = 0; i < 240 * 240; i++) fb_[i] = col;
  } else if (gfx_) {
    gfx_->fillScreen(col);
  }
}

void LcdCompositor::fillRect(int x, int y, int w, int h, uint16_t col) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > 240) w = 240 - x;
  if (y + h > 240) h = 240 - y;
  if (w <= 0 || h <= 0) return;

  if (fb_) {
    for (int j = y; j < y + h; j++) {
      uint16_t *row = fb_ + j * 240 + x;
      for (int i = 0; i < w; i++) row[i] = col;
    }
  } else if (gfx_) {
    gfx_->fillRect(x, y, w, h, col);
  }
}

void LcdCompositor::drawTextScaled(const char *s, int x, int y, int scale, uint16_t col) {
  int cx = x;
  for (; *s; s++) {
    for (uint8_t r = 0; r < 5; r++) {
      uint8_t bits = ocwFont3x5Row(*s, r);
      for (int c = 0; c < 3; c++) {
        if (bits & (1 << (2 - c))) {
          fillRect(cx + c * scale, y + r * scale, scale, scale, col);
        }
      }
    }
    cx += 4 * scale;
  }
}

void LcdCompositor::drawRecessedBezel(int ox, int oy, int lcdW, int lcdH, int border) {
  const LcdTheme &t = theme();
  fillRect(ox - border, oy - border, lcdW + border * 2, lcdH + border * 2, t.bezelOut);
  int inner = (border >= 4) ? (border / 2) : 1;
  fillRect(ox - inner, oy - inner, lcdW + inner * 2, lcdH + inner * 2, t.bezelIn);
}

void LcdCompositor::renderLcd32(const Lcd32 &lcd, int ox, int oy, int scale, bool withGrid) {
  const LcdTheme &t = theme();
  int size = 32 * scale;
  int cell = (withGrid && scale >= 4) ? (scale - 1) : scale;
  if (withGrid && scale >= 4) {
    fillRect(ox, oy, size, size, t.gridCol);
  }
  for (int py = 0; py < 32; py++) {
    int sy = oy + py * scale;
    for (int px = 0; px < 32; px++) {
      uint8_t lvl = lcd.pixelLevel(px, py);
      uint16_t col = blend565(t.offCol, t.onCol, lvl);
      int sx = ox + px * scale;
      if (fb_) {
        for (int dy = 0; dy < cell; dy++) {
          int yy = sy + dy;
          if (yy < 0 || yy >= 240) continue;
          uint16_t *dst = fb_ + yy * 240 + sx;
          for (int dx = 0; dx < cell; dx++) {
            if (sx + dx >= 0 && sx + dx < 240) dst[dx] = col;
          }
        }
      } else if (gfx_) {
        gfx_->fillRect(sx, sy, cell, cell, col);
      }
    }
  }
}

void LcdCompositor::present() {
  if (fb_ && gfx_) {
    gfx_->draw16bitRGBBitmap(0, 0, fb_, 240, 240);
  }
}

}  // namespace ocw
