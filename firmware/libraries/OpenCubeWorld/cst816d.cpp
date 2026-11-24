/*
 * cst816d.cpp - see cst816d.h. Vendored from the official Waveshare
 * 1.54inch Touch LCD Module example (unmodified).
 */
#include "cst816d.h"

static bool cst816d_read_flag = true;
static bool cst816d_int_enable = false;

static bool cst816d_read(uint8_t driver_addr, uint8_t reg_addr, uint8_t *data, uint32_t length) {
  Wire.beginTransmission(driver_addr);
  Wire.write(reg_addr);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  Wire.requestFrom(driver_addr, length);
  if (Wire.available() != (int)length) {
    return false;
  }
  Wire.readBytes(data, length);
  return true;
}

bool get_touch_data(touch_data_t *touch_data) {
  uint8_t buff[12];
  uint8_t touch_num = 0;

  if (cst816d_int_enable) {
    if (false == cst816d_read_flag) {
      return false;
    } else {
      cst816d_read_flag = false;
    }
  }

  cst816d_read(CST816D_ADDR, CST816D_TOUCH_NUM_REG, &touch_num, 1);
  touch_data->touch_num = touch_num;
  if (touch_num == 0)
    return false;

  cst816d_read(CST816D_ADDR, CST816D_TOUCH_XH_REG, &buff[0], 1);
  cst816d_read(CST816D_ADDR, CST816D_TOUCH_XL_REG, &buff[1], 1);
  cst816d_read(CST816D_ADDR, CST816D_TOUCH_YH_REG, &buff[2], 1);
  cst816d_read(CST816D_ADDR, CST816D_TOUCH_YL_REG, &buff[3], 1);
  for (uint16_t i = 0; i < touch_num; i++) {
    touch_data->coords[i].x = (((uint16_t)buff[(i * 6) + 0] & 0x0f) << 8) + buff[(i * 6) + 1];
    touch_data->coords[i].y = (((uint16_t)buff[(i * 6) + 2] & 0x0f) << 8) + buff[(i * 6) + 3];
  }
  return true;
}

void cst816d_touch_int_cb(void) {
  cst816d_read_flag = true;
}

void cst816d_init(int tp_rst, int tp_int) {
  uint8_t id = 0;
  Wire.begin();

  if (0 < tp_int) {
    digitalWrite(tp_rst, LOW);
    delay(200);
    digitalWrite(tp_rst, HIGH);
    delay(300);
  }
  if (0 < tp_int) {
    cst816d_int_enable = true;
    attachInterrupt(digitalPinToInterrupt(tp_int), cst816d_touch_int_cb, RISING);
  }
  cst816d_read(CST816D_ADDR, CST816D_ID_REG, &id, 1);
  if (0 != id) {
    Serial.print("CST816D id: ");
    Serial.println(id);
  }
}
