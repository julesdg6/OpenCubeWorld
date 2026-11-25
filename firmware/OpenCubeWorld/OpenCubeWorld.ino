/*
 * OpenCubeWorld.ino - Authentic 32x32 1-bit monochrome Radica Cube World
 * for the Waveshare ESP32-S3-LCD-1.54 (and ESP32-S3-Touch-LCD-1.54).
 *
 * Hardware (verified against waveshareteam/ESP32-S3-Touch-LCD-1.54):
 *   - 1.54" 240x240 ST7789 IPS SPI LCD (DC=45, CS=21, SCK=38, MOSI=39, RST=40, BL=46)
 *   - QMI8658 6-axis IMU on I2C (SDA=42, SCL=41, addr 0x6B): tilt, flip tumble, shake dizzy
 *   - 3 physical push buttons (matching the 3 buttons on a real Radica Cube World!):
 *       * Left  (GPIO 0 / BOOT): Short = cycle character, Long = cycle LCD palette (STN/B&W/INV)
 *       * Center(GPIO 5 / PWR) : Trigger character signature trick / whip / debug pattern
 *       * Right (GPIO 4 / KEY) : Short = cycle view mode (Single Cube / Linked World / Debug)
 *                                Long  = trigger Linked-Cube transfer (Venetian blind!)
 *   - Optional CST816 capacitive touch (I2C 0x15, auto-detected if present)
 *
 * Serial USB-CDC commands (115200 baud):
 *   s = status, c = character, p = palette, a = action/trick, m = mode,
 *   f = simulate flip tumble, z = simulate shake dizzy, t = transfer blind
 */
#include <OpenCubeWorld.h>

static Arduino_GFX *gfx = nullptr;
static ocw::World *worldPtr = nullptr;
static ocw::Qmi8658 imu;

// --- Simulated protocol traffic for Linked World mode --------------------
static uint32_t nextKeepAt = 0;
static uint32_t nextWhipAt = 0;
static int keepIdx = 0;
static uint8_t pktBuf[7] = {0};

// --- Debounced 3-button state (active LOW, internal pull-up) -------------
struct Btn {
  int pin;
  bool lastRaw;
  bool stable;
  uint32_t changedAt;
  uint32_t pressedAt;
  bool longFired;
};

static Btn btns[3] = {
    {OCW_BTN_LEFT_PIN, true, true, 0, 0, false},
    {OCW_BTN_CENTER_PIN, true, true, 0, 0, false},
    {OCW_BTN_RIGHT_PIN, true, true, 0, 0, false},
};

static void initButtons() {
  for (auto &b : btns) {
    if (b.pin >= 0) {
      pinMode(b.pin, INPUT_PULLUP);
      b.lastRaw = b.stable = digitalRead(b.pin);
    }
  }
}

static void handleButtonPress(int idx, bool isLong) {
  if (!worldPtr) return;
  if (idx == 0) {
    if (isLong) {
      worldPtr->cyclePalette();
      Serial.printf("[BTN1-LONG] Palette -> %s\n", worldPtr->paletteName());
    } else {
      worldPtr->cycleCharacter();
      Serial.printf("[BTN1] Character -> %s\n", worldPtr->activeCharName());
    }
  } else if (idx == 1) {
    if (isLong) {
      worldPtr->triggerShake();
      Serial.println("[BTN2-LONG] Dizzy!");
    } else {
      worldPtr->triggerAction();
      Serial.println("[BTN2] Action!");
    }
  } else if (idx == 2) {
    if (isLong) {
      worldPtr->triggerTransfer();
      Serial.println("[BTN3-LONG] Transfer!");
    } else {
      worldPtr->cycleViewMode();
      Serial.printf("[BTN3] Mode -> %d\n", (int)worldPtr->viewMode());
    }
  }
}

static void pollButtons(uint32_t now) {
  for (int i = 0; i < 3; i++) {
    Btn &b = btns[i];
    if (b.pin < 0) continue;
    bool raw = digitalRead(b.pin);
    if (raw != b.lastRaw) {
      b.lastRaw = raw;
      b.changedAt = now;
    }
    if ((now - b.changedAt) >= 35 && raw != b.stable) {
      b.stable = raw;
      if (!b.stable) {
        // Pressed (LOW)
        b.pressedAt = now;
        b.longFired = false;
      } else {
        // Released (HIGH)
        if (!b.longFired) {
          handleButtonPress(i, false);
        }
      }
    }
    if (!b.stable && !b.longFired && (now - b.pressedAt >= 650)) {
      b.longFired = true;
      handleButtonPress(i, true);
    }
  }
}

static void pollTouch(uint32_t now) {
  static uint32_t lastTouch = 0;
  touch_data_t td;
  if (!get_touch_data(&td) || td.touch_num == 0) return;
  if (now - lastTouch < 350) return;
  lastTouch = now;

  int16_t tx = td.coords[0].x;
  int16_t ty = td.coords[0].y;
  if (ty < 40) {
    if (tx < 80) worldPtr->cycleCharacter();
    else if (tx > 160) worldPtr->cycleViewMode();
    else worldPtr->cyclePalette();
  } else {
    worldPtr->triggerAction();
  }
}

static void pollSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    switch (c) {
      case 's':
      case 'S': {
        const auto &st = imu.state();
        Serial.printf("OCW status: char=%s pal=%s mode=%d imu=%s(addr=0x%02X ax=%.2f ay=%.2f az=%.2f) touch=%s\n",
                      worldPtr->activeCharName(), worldPtr->paletteName(),
                      (int)worldPtr->viewMode(),
                      st.ok ? "OK" : "NONE", imu.address(),
                      st.ax, st.ay, st.az,
                      cst816d_present() ? "OK" : "NONE");
        break;
      }
      case 'c': case 'C': worldPtr->cycleCharacter(); break;
      case 'p': case 'P': worldPtr->cyclePalette(); break;
      case 'a': case 'A': worldPtr->triggerAction(); break;
      case 'm': case 'M': worldPtr->cycleViewMode(); break;
      case 'f': case 'F': worldPtr->triggerFlip(); break;
      case 'z': case 'Z': worldPtr->triggerShake(); break;
      case 't': case 'T': worldPtr->triggerTransfer(); break;
      default: break;
    }
  }
}

static void stepLinkedSimulation(uint32_t now) {
  if (worldPtr->viewMode() != ocw::VIEW_LINKED_WORLD) return;
  if (now >= nextKeepAt) {
    int slot = keepIdx % 2;
    uint8_t id = ocw::kCharDefaultIds[slot];
    CubeProtocol::mkKeepalive((uint8_t)slot, id, pktBuf);
    worldPtr->lastPacket = pktBuf;
    worldPtr->lastPacketAt = now;
    worldPtr->onKeepalive((uint8_t)slot, id);
    keepIdx++;
    nextKeepAt = now + 450;
  }
  if (now >= nextWhipAt) {
    CubeProtocol::mkAction(random(2) + 1, random(2) * 2, random(12) + 2,
                           ocw::kCharDefaultIds[0], pktBuf);
    worldPtr->lastPacket = pktBuf;
    worldPtr->lastPacketAt = now;
    worldPtr->onWhipIdx(0, 1);
    nextWhipAt = now + 4800 + random(3500);
  }
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("\n=== OpenCubeWorld (Authentic 32x32 Monochrome LCD) ===");

  gfx = ocwCreateDisplay();
  if (!gfx->begin()) {
    Serial.println("ERROR: ST7789 gfx->begin() failed!");
  }
  ocwBacklightOn();

  initButtons();

  bool imuOk = imu.begin(Wire, OCW_I2C_SDA_PIN, OCW_I2C_SCL_PIN);
  Serial.printf("QMI8658 IMU: %s (addr=0x%02X, SDA=%d SCL=%d)\n",
                imuOk ? "OK" : "NOT FOUND", imu.address(),
                OCW_I2C_SDA_PIN, OCW_I2C_SCL_PIN);

  cst816d_init(OCW_TP_RST_PIN, OCW_TP_INT_PIN, OCW_I2C_SDA_PIN, OCW_I2C_SCL_PIN);

  worldPtr = new ocw::World(gfx);
  worldPtr->begin();

#if defined(ESP32)
  randomSeed(esp_random());
#else
  randomSeed(analogRead(A0));
#endif

  nextKeepAt = millis() + 800;
  nextWhipAt = millis() + 4500;
  Serial.println("OpenCubeWorld ready! Send 's' over serial for telemetry.");
}

void loop() {
  uint32_t now = millis();

  pollButtons(now);
  pollTouch(now);
  pollSerial();

  static uint32_t lastImuAt = 0;
  if (now - lastImuAt >= 30) {
    lastImuAt = now;
    if (imu.update(now)) {
      worldPtr->setImu(imu.state());
    }
  }

  stepLinkedSimulation(now);

  worldPtr->update(now);
  worldPtr->draw();
  delay(33);  // ~30 fps smooth STN persistence & flicker-free framebuffer flush
}
