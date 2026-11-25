/*
 * config.h - pin maps for OpenCubeWorld.
 *
 * Default on ESP32-S3 (OCW_BOARD_WAVESHARE_ESP32S3_LCD_154):
 *   Waveshare ESP32-S3-LCD-1.54 / ESP32-S3-Touch-LCD-1.54
 *   (ST7789 240x240 IPS SPI LCD, QMI8658 6-axis IMU, 3 physical buttons,
 *    optional CST816 capacitive touch on shared I2C bus).
 *
 * Fallback on AVR (__AVR__):
 *   Arduino Uno wired to Waveshare 1.54inch Touch LCD Module (SKU 34370).
 *
 * Override any OCW_* pin by #define-ing it before including OpenCubeWorld.h.
 */
#pragma once

#if defined(OCW_BOARD_WAVESHARE_ESP32S3_LCD_154) || defined(ESP32)

// ---- display (ST7789, 4-wire SPI) --------------------------------------
#ifndef OCW_LCD_CS_PIN
#define OCW_LCD_CS_PIN 21
#endif
#ifndef OCW_LCD_DC_PIN
#define OCW_LCD_DC_PIN 45
#endif
#ifndef OCW_LCD_RST_PIN
#define OCW_LCD_RST_PIN 40
#endif
#ifndef OCW_LCD_SCLK_PIN
#define OCW_LCD_SCLK_PIN 38
#endif
#ifndef OCW_LCD_MOSI_PIN
#define OCW_LCD_MOSI_PIN 39
#endif
#ifndef OCW_LCD_BL_PIN
#define OCW_LCD_BL_PIN 46
#endif

// ---- shared I2C bus (QMI8658 IMU + optional CST816 touch + ES8311) -----
#ifndef OCW_I2C_SDA_PIN
#define OCW_I2C_SDA_PIN 42
#endif
#ifndef OCW_I2C_SCL_PIN
#define OCW_I2C_SCL_PIN 41
#endif

// ---- optional touch (CST816, I2C @ 0x15, Touch variant only) -----------
#ifndef OCW_TP_RST_PIN
#define OCW_TP_RST_PIN 47
#endif
#ifndef OCW_TP_INT_PIN
#define OCW_TP_INT_PIN 48
#endif

// ---- 3 physical push buttons (active LOW, internal pull-up) ------------
#ifndef OCW_BTN_LEFT_PIN
#define OCW_BTN_LEFT_PIN 0    // BOOT button
#endif
#ifndef OCW_BTN_CENTER_PIN
#define OCW_BTN_CENTER_PIN 5  // PWR / Center button
#endif
#ifndef OCW_BTN_RIGHT_PIN
#define OCW_BTN_RIGHT_PIN 4   // KEY / Right button
#endif

// ---- Cube World single data line (hardware link mode, 3.3V native!) ----
#ifndef OCW_CUBE_DATA_PIN
#define OCW_CUBE_DATA_PIN 11
#endif

#else
// ---- Arduino Uno fallback (Waveshare 1.54" SKU 34370) ------------------
#ifndef OCW_LCD_CS_PIN
#define OCW_LCD_CS_PIN 9
#endif
#ifndef OCW_LCD_DC_PIN
#define OCW_LCD_DC_PIN 8
#endif
#ifndef OCW_LCD_RST_PIN
#define OCW_LCD_RST_PIN 7
#endif
#ifndef OCW_LCD_SCLK_PIN
#define OCW_LCD_SCLK_PIN 13
#endif
#ifndef OCW_LCD_MOSI_PIN
#define OCW_LCD_MOSI_PIN 11
#endif
#ifndef OCW_LCD_BL_PIN
#define OCW_LCD_BL_PIN 6
#endif
#ifndef OCW_I2C_SDA_PIN
#define OCW_I2C_SDA_PIN -1
#endif
#ifndef OCW_I2C_SCL_PIN
#define OCW_I2C_SCL_PIN -1
#endif
#ifndef OCW_TP_RST_PIN
#define OCW_TP_RST_PIN 2
#endif
#ifndef OCW_TP_INT_PIN
#define OCW_TP_INT_PIN 3
#endif
#ifndef OCW_BTN_LEFT_PIN
#define OCW_BTN_LEFT_PIN -1
#endif
#ifndef OCW_BTN_CENTER_PIN
#define OCW_BTN_CENTER_PIN -1
#endif
#ifndef OCW_BTN_RIGHT_PIN
#define OCW_BTN_RIGHT_PIN -1
#endif
#ifndef OCW_CUBE_DATA_PIN
#define OCW_CUBE_DATA_PIN 4
#endif
#endif

// ---- display geometry --------------------------------------------------
#define OCW_SCREEN_W 240
#define OCW_SCREEN_H 240
