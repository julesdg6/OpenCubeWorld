/*
 * config.h - pin map for the Waveshare 1.54inch Touch LCD Module
 * (SKU 34370) + Cube World data line.
 *
 * Default pinout: Arduino Uno (matches the official Waveshare
 * "Arduino-uno" example). Override any OCW_* pin by redefining it
 * in your sketch (or in platformio_build_flags / boards.txt) before
 * including OpenCubeWorld.h, e.g. for an ESP32-S3:
 *
 *   #define OCW_LCD_CS_PIN 5
 *   #define OCW_LCD_DC_PIN 4
 *   ...
 */
#pragma once

// ---- display (ST7789S, 4-wire SPI) --------------------------------------
// 12PIN cable: MOSI=3 SCLK=4 LCD_CS=5 LCD_DC=6 LCD_RST=7 LCD_BL=8
#ifndef OCW_LCD_CS_PIN
#define OCW_LCD_CS_PIN 9      // Uno: CS
#endif
#ifndef OCW_LCD_DC_PIN
#define OCW_LCD_DC_PIN 8      // Uno: DC
#endif
#ifndef OCW_LCD_RST_PIN
#define OCW_LCD_RST_PIN 7     // Uno: RST
#endif
#ifndef OCW_LCD_SCLK_PIN
#define OCW_LCD_SCLK_PIN 13   // Uno: SCK
#endif
#ifndef OCW_LCD_MOSI_PIN
#define OCW_LCD_MOSI_PIN 11   // Uno: MOSI
#endif
#ifndef OCW_LCD_BL_PIN
#define OCW_LCD_BL_PIN 6      // Uno: backlight
#endif

// ---- touch (CST816D, I2C @ 0x15) ------------------------------------------
#ifndef OCW_TP_RST_PIN
#define OCW_TP_RST_PIN 2      // Uno: TP_RST
#endif
#ifndef OCW_TP_INT_PIN
#define OCW_TP_INT_PIN 3      // Uno: TP_INT
#endif

// ---- Cube World single data line (hardware link mode) ---------------------
// Connect the toy's data line to this pin through a 3.3V level setup
// (the toy logic is 3.3V; AVR pins are 5V - see docs/hardware.md).
#ifndef OCW_CUBE_DATA_PIN
#define OCW_CUBE_DATA_PIN 4
#endif

// ---- display geometry ------------------------------------------------------
#define OCW_SCREEN_W 240
#define OCW_SCREEN_H 240
