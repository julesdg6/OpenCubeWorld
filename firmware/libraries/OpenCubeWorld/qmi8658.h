/*
 * qmi8658.h - Lightweight zero-dependency I2C driver for the QMI8658 6-axis IMU
 * on the Waveshare ESP32-S3-LCD-1.54 (I2C SDA=42, SCL=41, addr 0x6B/0x6A).
 *
 * Provides raw accel/gyro readings plus Radica Cube World gesture detectors:
 *   - tiltX / tiltY (-1.0 .. +1.0 g)
 *   - upside-down flip detection (triggers CUBE TURN tumble)
 *   - shake detection (triggers DIZZY/SICK stagger)
 */
#pragma once
#include <Arduino.h>
#include <Wire.h>

namespace ocw {

struct ImuState {
  bool ok = false;
  float ax = 0.0f, ay = 0.0f, az = 1.0f;  // g
  float gx = 0.0f, gy = 0.0f, gz = 0.0f;  // deg/s
  float tiltX = 0.0f;                     // filtered horizontal tilt (-1..+1)
  float shakeEnergy = 0.0f;               // decaying shake accumulator
  bool upsideDown = false;
  bool flipEvent = false;                 // rising edge when flipped upside down
  bool shakeEvent = false;                // rising edge when shaken vigorously
};

class Qmi8658 {
 public:
  bool begin(TwoWire &wire = Wire, int sda = -1, int scl = -1);
  bool update(uint32_t now_ms);
  const ImuState &state() const { return st_; }
  uint8_t address() const { return addr_; }

 private:
  TwoWire *wire_ = &Wire;
  uint8_t addr_ = 0x6B;
  ImuState st_;
  float prevMag_ = 1.0f;
  uint32_t lastFlipAt_ = 0;
  uint32_t lastShakeAt_ = 0;
  bool prevUpsideDown_ = false;

  bool writeReg(uint8_t reg, uint8_t val);
  bool readRegs(uint8_t reg, uint8_t *buf, size_t len);
};

}  // namespace ocw
