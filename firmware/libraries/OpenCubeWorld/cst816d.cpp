/*
 * cst816d.cpp - CST816D/CST816S capacitive touch driver with presence detection.
 */
#include "cst816d.h"

static bool cst816d_read_flag = true;
static bool cst816d_int_enable = false;
static bool cst816d_detected = false;

static bool cst816d_read(uint8_t driver_addr, uint8_t reg_addr, uint8_t *data, uint32_t length) {
  Wire.beginTransmission(driver_addr);
  Wire.write(reg_addr);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  Wire.requestFrom(driver_addr, (uint8_t)length);
  if (Wire.available() != (int)length) {
    return false;
  }
  Wire.readBytes(data, length);
  return true;
}

bool cst816d_present() {
  return cst816d_detected;
}

bool get_touch_data(touch_data_t *touch_data) {
  if (!cst816d_detected) return false;

  uint8_t buff[6] = {0};
  uint8_t touch_num = 0;

  if (cst816d_int_enable) {
    if (!cst816d_read_flag) return false;
    cst816d_read_flag = false;
  }

  if (!cst816d_read(CST816D_ADDR, CST816D_TOUCH_NUM_REG, &touch_num, 1)) {
    return false;
  }
  touch_data->touch_num = touch_num;
  if (touch_num == 0 || touch_num > 2) return false;

  if (!cst816d_read(CST816D_ADDR, CST816D_TOUCH_XH_REG, buff, 4)) {
    return false;
  }
  touch_data->coords[0].x = (((uint16_t)buff[0] & 0x0F) << 8) | buff[1];
  touch_data->coords[0].y = (((uint16_t)buff[2] & 0x0F) << 8) | buff[3];
  return true;
}

void IRAM_ATTR cst816d_touch_int_cb(void) {
  cst816d_read_flag = true;
}

bool cst816d_init(int tp_rst, int tp_int, int sda, int scl) {
  uint8_t id = 0;
#if defined(ESP32)
  if (sda >= 0 && scl >= 0) {
    Wire.begin(sda, scl);
  } else {
    Wire.begin();
  }
#else
  (void)sda;
  (void)scl;
  Wire.begin();
#endif

  if (tp_rst >= 0) {
    pinMode(tp_rst, OUTPUT);
    digitalWrite(tp_rst, LOW);
    delay(20);
    digitalWrite(tp_rst, HIGH);
    delay(60);
  }

  // Probe chip ID; if absent (non-touch ESP32-S3-LCD-1.54), gracefully disable.
  cst816d_detected = cst816d_read(CST816D_ADDR, CST816D_ID_REG, &id, 1) && (id != 0);
  if (cst816d_detected) {
    if (tp_int >= 0) {
      pinMode(tp_int, INPUT_PULLUP);
      cst816d_int_enable = true;
      attachInterrupt(digitalPinToInterrupt(tp_int), cst816d_touch_int_cb, FALLING);
    }
    Serial.print("CST816 touch detected, id=0x");
    Serial.println(id, HEX);
  } else {
    Serial.println("CST816 touch not present (button/IMU mode active)");
  }
  return cst816d_detected;
}
