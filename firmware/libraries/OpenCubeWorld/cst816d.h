/*
 * cst816d.h - CST816D/CST816S capacitive touch driver (I2C @ 0x15).
 * Auto-detects hardware presence so non-touch Waveshare ESP32-S3-LCD-1.54
 * boards run cleanly without blocking.
 */
#pragma once
#include <Arduino.h>
#include <Wire.h>

#define CST816D_ADDR           0x15
#define CST816D_TOUCH_NUM_REG  0x02
#define CST816D_TOUCH_XH_REG   0x03
#define CST816D_TOUCH_XL_REG   0x04
#define CST816D_TOUCH_YH_REG   0x05
#define CST816D_TOUCH_YL_REG   0x06
#define CST816D_ID_REG         0xA7

struct touch_coord_t {
  uint16_t x;
  uint16_t y;
};

struct touch_data_t {
  uint8_t touch_num;
  touch_coord_t coords[2];
};

// Returns true if CST816 hardware responded on I2C.
bool cst816d_init(int tp_rst, int tp_int, int sda = -1, int scl = -1);
bool cst816d_present();
bool get_touch_data(touch_data_t *touch_data);
