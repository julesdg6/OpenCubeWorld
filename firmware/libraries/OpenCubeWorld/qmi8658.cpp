/*
 * qmi8658.cpp - Lightweight QMI8658 6-axis IMU driver for Waveshare ESP32-S3-LCD-1.54.
 */
#include "qmi8658.h"
#include <math.h>

namespace ocw {

static constexpr uint8_t REG_WHO_AM_I = 0x00;
static constexpr uint8_t REG_CTRL1    = 0x02;
static constexpr uint8_t REG_CTRL2    = 0x03;
static constexpr uint8_t REG_CTRL3    = 0x04;
static constexpr uint8_t REG_CTRL5    = 0x06;
static constexpr uint8_t REG_CTRL7    = 0x08;
static constexpr uint8_t REG_AX_L     = 0x35;  // 12 bytes: AX..GZ little-endian int16

bool Qmi8658::writeReg(uint8_t reg, uint8_t val) {
  wire_->beginTransmission(addr_);
  wire_->write(reg);
  wire_->write(val);
  return wire_->endTransmission() == 0;
}

bool Qmi8658::readRegs(uint8_t reg, uint8_t *buf, size_t len) {
  wire_->beginTransmission(addr_);
  wire_->write(reg);
  if (wire_->endTransmission(false) != 0) return false;
  if (wire_->requestFrom(addr_, (uint8_t)len) != (int)len) return false;
  for (size_t i = 0; i < len; i++) buf[i] = wire_->read();
  return true;
}

bool Qmi8658::begin(TwoWire &wire, int sda, int scl) {
  wire_ = &wire;
#if defined(ESP32)
  if (sda >= 0 && scl >= 0) {
    wire_->begin(sda, scl);
  } else {
    wire_->begin();
  }
#else
  (void)sda;
  (void)scl;
  wire_->begin();
#endif

  // Probe both standard QMI8658 I2C addresses (0x6B = SA0 low, 0x6A = SA0 high)
  const uint8_t candidates[2] = {0x6B, 0x6A};
  bool found = false;
  for (uint8_t a : candidates) {
    addr_ = a;
    uint8_t who = 0;
    if (readRegs(REG_WHO_AM_I, &who, 1) && who == 0x05) {
      found = true;
      break;
    }
  }
  st_.ok = found;
  if (!found) return false;

  // CTRL1: auto-increment address (bit 6 = 1), little-endian (bit 5 = 0)
  writeReg(REG_CTRL1, 0x40);
  // CTRL2: Accel +/-4g (bits 6:4 = 001 -> 0x10), ODR 250Hz (bits 3:0 = 0x05)
  writeReg(REG_CTRL2, 0x15);
  // CTRL3: Gyro +/-512dps (bits 6:4 = 101 -> 0x50), ODR 250Hz (bits 3:0 = 0x05)
  writeReg(REG_CTRL3, 0x55);
  // CTRL5: Low-pass filter enabled for accel & gyro
  writeReg(REG_CTRL5, 0x11);
  // CTRL7: Enable Accel (bit 0) + Gyro (bit 1)
  writeReg(REG_CTRL7, 0x03);
  delay(10);
  return true;
}

bool Qmi8658::update(uint32_t now_ms) {
  st_.flipEvent = false;
  st_.shakeEvent = false;
  if (!st_.ok) return false;

  uint8_t raw[12];
  if (!readRegs(REG_AX_L, raw, sizeof(raw))) return false;

  auto i16 = [&](int off) -> int16_t {
    return (int16_t)((uint16_t)raw[off] | ((uint16_t)raw[off + 1] << 8));
  };

  // +/-4g -> 8192 LSB/g; +/-512dps -> 64 LSB/dps
  st_.ax = i16(0) / 8192.0f;
  st_.ay = i16(2) / 8192.0f;
  st_.az = i16(4) / 8192.0f;
  st_.gx = i16(6) / 64.0f;
  st_.gy = i16(8) / 64.0f;
  st_.gz = i16(10) / 64.0f;

  // Smooth horizontal tilt for gentle leaning/sliding
  float rawTilt = constrain(st_.ax, -1.0f, 1.0f);
  st_.tiltX = st_.tiltX * 0.78f + rawTilt * 0.22f;

  // Upside-down detection (vertical axis strongly inverted on upright cube)
  bool invNow = (st_.ay < -0.68f) || (st_.az < -0.85f);
  st_.upsideDown = invNow;
  if (invNow && !prevUpsideDown_ && (now_ms - lastFlipAt_ > 1800)) {
    st_.flipEvent = true;
    lastFlipAt_ = now_ms;
  }
  prevUpsideDown_ = invNow;

  // Shake detection via acceleration delta + angular velocity magnitude
  float mag = sqrtf(st_.ax * st_.ax + st_.ay * st_.ay + st_.az * st_.az);
  float delta = fabsf(mag - prevMag_);
  prevMag_ = mag;
  float gyroMag = fabsf(st_.gx) + fabsf(st_.gy) + fabsf(st_.gz);
  if (delta > 0.45f || gyroMag > 360.0f) {
    st_.shakeEnergy += delta * 1.4f + (gyroMag > 360.0f ? 0.35f : 0.0f);
  }
  st_.shakeEnergy *= 0.86f;
  if (st_.shakeEnergy > 1.85f && (now_ms - lastShakeAt_ > 2500)) {
    st_.shakeEvent = true;
    st_.shakeEnergy = 0.0f;
    lastShakeAt_ = now_ms;
  }
  return true;
}

}  // namespace ocw
