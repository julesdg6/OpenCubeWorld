/*
 * world.cpp - Authentic 32x32 1-bit monochrome Radica Cube World display engine.
 */
#include "world.h"
#include "cubeprotocol.h"
#include <stdio.h>
#include <string.h>

namespace ocw {

const char *const kCharNames[N_CHARS] = {
    "WHIP", "DODGER", "HANS", "HANDY", "SCOOP",
};

const uint8_t kCharDefaultIds[N_CHARS] = {0x06, 0x0E, 0x0D, 0x0B, 0x01};

World::World(Arduino_GFX *gfx) : gfx_(gfx), comp_(gfx) {}

World::~World() {}

void World::begin() {
  if (gfx_) {
    gfx_->setRotation(0);
  }
  comp_.begin();
  comp_.setPalette(LCD_PAL_STN);
  setup(2);
  bootStart_ = millis();
  bootDone_ = false;
  lastActivity_ = bootStart_;
  nextAutoAnim_ = bootStart_ + 3500;
  session = SESS_ACTIVE;
  showToast(kCharNames[cubes_[0].charIdx], 1400);
}

void World::setup(int count) {
  nCubes = constrain(count, 1, MAX_CUBES);
  static const int roster[2] = {CHAR_WHIP, CHAR_DODGER};
  for (int i = 0; i < MAX_CUBES; i++) {
    Cube &c = cubes_[i];
    c.present = (i < nCubes);
    c.charIdx = roster[i % 2];
    c.id = kCharDefaultIds[c.charIdx];
    c.slot = (uint8_t)i;
    c.posX = 10;
    c.walkDir = (i == 0) ? 1 : -1;
    c.facingLeft = (i == 1);
    c.anim = ANIM_IDLE;
    c.animStart = millis();
    c.animUntil = 0;
    c.hopUntil = 0;
    c.vacated = false;
    c.blindRows = 0;
    speech_[i][0] = 0;
    speechUntil_[i] = 0;
  }
  visiting_ = false;
  visitUntil_ = 0;
  lastPacket = nullptr;
  hasLastPacket = false;
}

void World::resetToBoot() {
  bootStart_ = millis();
  bootDone_ = false;
  session = SESS_BOOT;
  setup(nCubes);
}

void World::showToast(const char *msg, uint32_t ms) {
  strncpy(toast_, msg, sizeof(toast_) - 1);
  toast_[sizeof(toast_) - 1] = 0;
  toastUntil_ = millis() + ms;
}

void World::say(int idx, const char *msg, uint32_t ms) {
  if (idx < 0 || idx >= MAX_CUBES) return;
  strncpy(speech_[idx], msg, sizeof(speech_[idx]) - 1);
  speech_[idx][sizeof(speech_[idx]) - 1] = 0;
  speechUntil_[idx] = millis() + ms;
}

void World::setAnim(int idx, Anim a, uint32_t durMs, uint32_t now) {
  if (idx < 0 || idx >= MAX_CUBES) return;
  cubes_[idx].anim = a;
  cubes_[idx].animStart = now;
  cubes_[idx].animUntil = durMs ? (now + durMs) : 0;
}

void World::wakeUp() {
  uint32_t now = millis();
  lastActivity_ = now;
  for (int i = 0; i < MAX_CUBES; i++) {
    if (cubes_[i].anim == ANIM_SLEEP) {
      setAnim(i, ANIM_STRETCH, 1400, now);
    }
  }
}

void World::setViewMode(ViewMode mode) {
  view_ = (ViewMode)(mode % VIEW_N_MODES);
  wakeUp();
  if (view_ == VIEW_SINGLE_CUBE) showToast(kCharNames[cubes_[0].charIdx], 1000);
  else if (view_ == VIEW_LINKED_WORLD) showToast("LINK", 1000);
  else showToast("DBG", 1000);
}

void World::cycleViewMode() {
  setViewMode((ViewMode)((view_ + 1) % VIEW_N_MODES));
}

void World::cycleCharacter() {
  wakeUp();
  cubes_[0].charIdx = (cubes_[0].charIdx + 1) % N_CHARS;
  cubes_[0].id = kCharDefaultIds[cubes_[0].charIdx];
  // Pair a distinct neighbor in Linked mode
  cubes_[1].charIdx = (cubes_[0].charIdx + 1) % N_CHARS;
  cubes_[1].id = kCharDefaultIds[cubes_[1].charIdx];
  setAnim(0, ANIM_WAVE, 1100, millis());
  showToast(kCharNames[cubes_[0].charIdx], 1300);
}

void World::cyclePalette() {
  wakeUp();
  comp_.setPalette((LcdPalette)((comp_.palette() + 1) % LCD_N_PALS));
  showToast(comp_.theme().name, 1200);
}

void World::cycleDebugPage() {
  wakeUp();
  debugPage_ = (debugPage_ + 1) % 6;
}

void World::triggerAction() {
  uint32_t now = millis();
  wakeUp();
  if (view_ == VIEW_DEBUG) {
    cycleDebugPage();
    return;
  }
  if (view_ == VIEW_LINKED_WORLD) {
    onWhipIdx(0, 1);
    return;
  }
  setAnim(0, ANIM_TRICK, 2400, now);
}

void World::triggerFlip() {
  uint32_t now = millis();
  wakeUp();
  setAnim(0, ANIM_TUMBLE, 2200, now);
  if (view_ == VIEW_LINKED_WORLD) {
    setAnim(1, ANIM_TUMBLE, 2200, now);
  }
}

void World::triggerShake() {
  uint32_t now = millis();
  wakeUp();
  setAnim(0, ANIM_DIZZY, 2600, now);
  if (view_ == VIEW_LINKED_WORLD) {
    setAnim(1, ANIM_DIZZY, 2600, now);
  }
}

void World::triggerTransfer() {
  uint32_t now = millis();
  wakeUp();
  if (view_ != VIEW_LINKED_WORLD) return;
  visiting_ = !visiting_;
  cubes_[0].vacated = visiting_;
  if (visiting_) {
    visitUntil_ = now + 6500;
    say(1, "HI!", 1000);
    setAnim(1, ANIM_WAVE, 1200, now);
  } else {
    cubes_[0].blindRows = 0;
    setAnim(0, ANIM_WAVE, 900, now);
  }
}

void World::setImu(const ImuState &imu) {
  imu_ = imu;
  if (imu_.flipEvent) {
    triggerFlip();
  } else if (imu_.shakeEvent) {
    triggerShake();
  }
}

int World::findCube(uint8_t id) const {
  for (int i = 0; i < MAX_CUBES; i++)
    if (cubes_[i].present && cubes_[i].id == id) return i;
  return -1;
}

int World::nearestCube(int16_t x, int16_t y) const {
  (void)y;
  return (x < 120) ? 0 : 1;
}

// ------------------------------------------------------------ protocol events

void World::onHello(uint8_t id) {
  uint32_t now = millis();
  if (session == SESS_BOOT) session = SESS_HELLO;
  int i = findCube(id);
  if (i < 0) i = 0;
  setAnim(i, ANIM_WAVE, 1300, now);
  say(i, "HI!", 1200);
}

void World::onKeepalive(uint8_t slot, uint8_t id) {
  (void)slot;
  uint32_t now = millis();
  if (session == SESS_HELLO) session = SESS_ACTIVE;
  int i = findCube(id);
  if (i >= 0) {
    cubes_[i].hopUntil = now + 180;
  }
}

void World::onWhip(uint8_t a1, uint8_t a2, uint8_t a3, uint8_t senderId,
                   int16_t targetX, int16_t targetY) {
  (void)a1; (void)a2; (void)a3;
  int s = findCube(senderId);
  if (s < 0) s = 0;
  int t = nearestCube(targetX, targetY);
  if (t == s) t = 1 - s;
  onWhipIdx(s, t);
}

void World::onWhipIdx(int senderIdx, int targetIdx) {
  uint32_t now = millis();
  if (session == SESS_HELLO) session = SESS_ACTIVE;
  int s = constrain(senderIdx, 0, MAX_CUBES - 1);
  int t = constrain(targetIdx, 0, MAX_CUBES - 1);
  if (t == s) t = 1 - s;
  cubes_[s].facingLeft = (s > t);
  cubes_[t].facingLeft = (t > s);
  setAnim(s, ANIM_WHIP, 1100, now);
  setAnim(t, ANIM_DODGE, 1100, now);
  say(t, "!!", 900);
}

void World::onBye(uint8_t id) {
  uint32_t now = millis();
  session = SESS_BYE;
  int i = findCube(id);
  if (i < 0) i = 0;
  setAnim(i, ANIM_WAVE, 1500, now);
  say(i, "BYE", 1400);
}

void World::onEnd(uint8_t id) {
  (void)id;
  uint32_t now = millis();
  session = SESS_END;
  for (int i = 0; i < MAX_CUBES; i++) {
    setAnim(i, ANIM_SIT, 0, now);
  }
}

// ------------------------------------------------------------------- update

void World::update(uint32_t now) {
  if (lastPacket) {
    memcpy(lastPacketCopy, lastPacket, 7);
    hasLastPacket = true;
  }

  // Power-on Radica maze wipe lasts ~1.5s
  if (!bootDone_ && (now - bootStart_ >= 1500)) {
    bootDone_ = true;
    setAnim(0, ANIM_WAVE, 1100, now);
  }

  // Update each cube's animation expiration & movement
  for (int i = 0; i < MAX_CUBES; i++) {
    Cube &c = cubes_[i];
    if (!c.present) continue;

    if (c.anim != ANIM_IDLE && c.anim != ANIM_SLEEP && c.animUntil > 0 && now >= c.animUntil) {
      c.anim = ANIM_IDLE;
      c.animStart = now;
      c.animUntil = 0;
    }

    // Gentle walking stride at choppy LCD cadence
    if (c.anim == ANIM_WALK && (now - lastRenderStep_ >= 220)) {
      c.posX += c.walkDir;
      if (c.posX <= 3) { c.posX = 3; c.walkDir = 1; c.facingLeft = false; }
      if (c.posX >= 17) { c.posX = 17; c.walkDir = -1; c.facingLeft = true; }
    }

    // Gentle IMU tilt response in Single Cube mode when standing/idle
    if (i == 0 && view_ == VIEW_SINGLE_CUBE && imu_.ok &&
        (c.anim == ANIM_IDLE || c.anim == ANIM_WALK) && fabsf(imu_.tiltX) > 0.30f) {
      int targetX = constrain(10 + (int)(imu_.tiltX * 9.0f), 2, 18);
      if (c.posX < targetX) { c.posX++; c.facingLeft = false; }
      else if (c.posX > targetX) { c.posX--; c.facingLeft = true; }
    }

    // Animate Venetian blind lowering/raising
    if (c.vacated) {
      if (c.blindRows < 28) c.blindRows += 2;
    } else {
      if (c.blindRows > 0) c.blindRows = (c.blindRows > 3) ? (uint8_t)(c.blindRows - 3) : 0;
    }
  }

  // End linked-cube visit after duration
  if (visiting_ && now >= visitUntil_) {
    visiting_ = false;
    cubes_[0].vacated = false;
    setAnim(0, ANIM_IDLE, 0, now);
  }

  // Autonomous Radica Cube World life engine (in Single Cube mode)
  if (bootDone_ && view_ == VIEW_SINGLE_CUBE) {
    Cube &c = cubes_[0];
    // Fall asleep after 90s of no button/IMU/touch interaction
    if (c.anim != ANIM_SLEEP && (now - lastActivity_ > 90000)) {
      setAnim(0, ANIM_SLEEP, 0, now);
    } else if (c.anim == ANIM_IDLE && now >= nextAutoAnim_) {
      static const Anim kPool[] = {
          ANIM_WALK, ANIM_STRETCH, ANIM_SIT, ANIM_ITCH,
          ANIM_KICK, ANIM_TRICK, ANIM_MAD, ANIM_WAVE,
      };
      Anim pick = kPool[random(sizeof(kPool) / sizeof(kPool[0]))];
      uint32_t dur = (pick == ANIM_SIT) ? 3200 : (pick == ANIM_WALK ? 2600 : 2200);
      if (pick == ANIM_WALK) {
        c.walkDir = random(2) ? 1 : -1;
        c.facingLeft = (c.walkDir < 0);
      }
      setAnim(0, pick, dur, now);
      nextAutoAnim_ = now + dur + 2200 + random(2600);
    }
  }

  if (now - lastRenderStep_ >= 220) {
    lastRenderStep_ = now;
  }
}

// ------------------------------------------------------- 32x32 LCD drawing

void World::drawStickmanOnLcd(const Cube &c, Lcd32 &lcd, uint32_t now, int xOverride) {
  uint32_t elapsed = now - c.animStart;
  int step = (int)(elapsed / 260);  // Authentic choppy ~3.8 fps Radica LCD cadence
  int pose = OCW_POSE_IDLE0;
  bool flip = c.facingLeft;
  int yOff = (c.hopUntil > now) ? -2 : 0;

  switch (c.anim) {
    case ANIM_IDLE: {
      static const uint8_t seq[6] = {OCW_POSE_IDLE0, OCW_POSE_IDLE0, OCW_POSE_IDLE1,
                                     OCW_POSE_IDLE0, OCW_POSE_IDLE2, OCW_POSE_IDLE0};
      pose = seq[(now / 550 + c.slot) % 6];
      break;
    }
    case ANIM_WALK: {
      static const uint8_t seq[4] = {OCW_POSE_WALK0, OCW_POSE_WALK1, OCW_POSE_WALK2, OCW_POSE_WALK3};
      pose = seq[step % 4];
      break;
    }
    case ANIM_STRETCH: {
      static const uint8_t seq[6] = {OCW_POSE_IDLE0, OCW_POSE_STRETCH0, OCW_POSE_STRETCH1,
                                     OCW_POSE_STRETCH1, OCW_POSE_STRETCH0, OCW_POSE_IDLE0};
      pose = seq[min(step, 5)];
      break;
    }
    case ANIM_SIT: {
      pose = (step == 0) ? OCW_POSE_SIT0 : OCW_POSE_SIT1;
      break;
    }
    case ANIM_ITCH: {
      pose = (step & 1) ? OCW_POSE_ITCH1 : OCW_POSE_ITCH0;
      break;
    }
    case ANIM_MAD: {
      static const uint8_t seq[4] = {OCW_POSE_MAD0, OCW_POSE_MAD1, OCW_POSE_MAD2, OCW_POSE_MAD1};
      pose = seq[step % 4];
      break;
    }
    case ANIM_KICK: {
      pose = (step & 1) ? OCW_POSE_KICK1 : OCW_POSE_KICK0;
      break;
    }
    case ANIM_WAVE: {
      pose = (step & 1) ? OCW_POSE_WAVE1 : OCW_POSE_WAVE0;
      break;
    }
    case ANIM_WHIP: {
      pose = (step & 1) ? OCW_POSE_WHIP1 : OCW_POSE_WHIP0;
      break;
    }
    case ANIM_DODGE: {
      pose = (step & 1) ? OCW_POSE_DODGE1 : OCW_POSE_DODGE0;
      yOff += 1;
      break;
    }
    case ANIM_TUMBLE: {
      static const uint8_t seq[8] = {
          OCW_POSE_TUMBLE_FALL, OCW_POSE_TUMBLE_FLAT, OCW_POSE_TUMBLE_FLAT,
          OCW_POSE_TUMBLE_FLAT, OCW_POSE_TUMBLE_PUSH, OCW_POSE_TUMBLE_KNEEL,
          OCW_POSE_IDLE1, OCW_POSE_IDLE0};
      pose = seq[min(step, 7)];
      break;
    }
    case ANIM_DIZZY: {
      static const uint8_t seq[4] = {OCW_POSE_DIZZY0, OCW_POSE_DIZZY1, OCW_POSE_DIZZY2, OCW_POSE_DIZZY1};
      pose = seq[step % 4];
      break;
    }
    case ANIM_TRICK: {
      // Character-specific Radica signature actions!
      switch (c.charIdx) {
        case CHAR_WHIP:
          pose = (step & 1) ? OCW_POSE_WHIP1 : OCW_POSE_WHIP0;
          break;
        case CHAR_DODGER:
          pose = (step & 1) ? OCW_POSE_KICK1 : OCW_POSE_KICK0;
          break;
        case CHAR_HANS:
          pose = (step & 1) ? OCW_POSE_STRETCH1 : OCW_POSE_STRETCH0;
          break;
        case CHAR_HANDY:
          pose = (step & 1) ? OCW_POSE_MAD1 : OCW_POSE_MAD0;
          break;
        default: // SCOOP / OPEN
          pose = (step & 1) ? OCW_POSE_WAVE0 : OCW_POSE_IDLE0;
          break;
      }
      break;
    }
    case ANIM_SLEEP: {
      // Authentic Radica Sleep: dark-inverted LCD, lying figure at bottom, rising Zs
      lcd.clear(true);
      const OcwSprite1B &flat = OCW_POSES[OCW_POSE_TUMBLE_FLAT];
      lcd.blit(flat, 6, 26, false, false);
      int zPhase = (now / 420) % 4;
      lcd.blit(OCW_PROPS[OCW_PROP_Z_SM], 18 + (zPhase & 1), 17 - zPhase * 2, false, false);
      if (zPhase >= 1) {
        lcd.blit(OCW_PROPS[OCW_PROP_Z_LG], 23, 11 - zPhase * 2, false, false);
      }
      return;
    }
  }

  const OcwSprite1B &sp = OCW_POSES[pose];
  int x = (xOverride != -99) ? xOverride : (int)c.posX;
  if (x + sp.w > 31) x = 31 - sp.w;
  if (x < 1) x = 1;
  int y = 30 - sp.h + yOff;
  if (y < 1) y = 1;
  lcd.blit(sp, x, y, flip, true);

  // Draw character-specific 1-bit monochrome props during TRICK or WHIP
  if (c.anim == ANIM_WHIP || (c.anim == ANIM_TRICK && c.charIdx == CHAR_WHIP)) {
    int f = step % 3;
    const OcwSprite1B &arc = OCW_PROPS[OCW_PROP_WHIP_ARC0 + f];
    int px = flip ? (x + sp.w - 1) : (x - arc.w + 2);
    lcd.blit(arc, px, y + 3, flip, true);
  } else if (c.anim == ANIM_TRICK) {
    switch (c.charIdx) {
      case CHAR_DODGER: {
        // Bouncing 1-bit pixel ball kicked into the air
        int bx = flip ? (x + sp.w + 1) : (x - 5);
        int by = 24 - ((step % 4) == 1 ? 10 : ((step % 4) == 2 ? 14 : ((step % 4) == 3 ? 6 : 0)));
        lcd.blit(OCW_PROPS[OCW_PROP_BALL], constrain(bx, 1, 26), constrain(by, 2, 26), false, true);
        break;
      }
      case CHAR_HANS: {
        // Overhead monochrome barbell
        int by = (step & 1) ? max(1, y - 4) : (y + 1);
        lcd.blit(OCW_PROPS[OCW_PROP_DUMBBELL], x + (sp.w - 11) / 2, by, false, true);
        break;
      }
      case CHAR_HANDY: {
        // Swinging wrench + dust spark
        int wx = flip ? (x + sp.w - 2) : max(1, x - 7);
        lcd.blit(OCW_PROPS[OCW_PROP_WRENCH], wx, y + 5 - (step & 1) * 3, flip, true);
        if (step & 1) lcd.blit(OCW_PROPS[OCW_PROP_DUST0], wx, y + 12, false, true);
        break;
      }
      case CHAR_OPEN: {
        // Scoop's dog Spot hopping beside him!
        const OcwSprite1B &dog = OCW_PROPS[(step & 1) ? OCW_PROP_DOG1 : OCW_PROP_DOG0];
        int dx = (x > 14) ? 2 : 19;
        int dy = 23 - ((step & 1) ? 2 : 0);
        lcd.blit(dog, dx, dy, x > 14, true);
        break;
      }
      default:
        break;
    }
  } else if (c.anim == ANIM_DODGE) {
    lcd.blit(OCW_PROPS[(step & 1) ? OCW_PROP_DUST1 : OCW_PROP_DUST0], x + 2, 26, false, true);
  }
}

void World::renderCubeToLcd(int idx, Lcd32 &lcd, uint32_t now, bool includeVisitor) {
  const Cube &c = cubes_[idx];
  lcd.clear(false);

  // Power-on maze wipe
  if (!bootDone_) {
    uint32_t dt = now - bootStart_;
    uint8_t prog = (dt < 900) ? (uint8_t)(dt * 32 / 900) : (uint8_t)(32 - (dt - 900) * 32 / 600);
    lcd.drawMazeBoot(prog);
    lcd.stepPersistence(false);
    return;
  }

  // Vacated cube shows authentic lowering Venetian blind
  if (c.vacated || c.blindRows > 0) {
    lcd.drawVenetianBlind(c.blindRows);
    lcd.stepPersistence(false);
    return;
  }

  // Draw resident stickman (and optional visiting friend in Linked mode)
  if (includeVisitor) {
    drawStickmanOnLcd(cubes_[0], lcd, now, 2);
    drawStickmanOnLcd(c, lcd, now, 17);
  } else {
    drawStickmanOnLcd(c, lcd, now);
  }

  // Tiny 3x5 monochrome speech tag (only on active protocol events)
  if (speechUntil_[idx] > now && speech_[idx][0]) {
    lcd.fillRect(1, 1, 14, 7, false);
    lcd.drawText(speech_[idx], 2, 2, true);
  }

  // Brief toast badge in Single Cube mode (top of 32x32 LCD, auto-hides)
  if (idx == 0 && view_ == VIEW_SINGLE_CUBE && toastUntil_ > now && toast_[0]) {
    int len = (int)strlen(toast_);
    int bw = len * 4 + 2;
    int bx = max(0, (32 - bw) / 2);
    lcd.fillRect(bx, 1, bw, 7, true);
    lcd.drawText(toast_, bx + 1, 2, false);
  }

  lcd.stepPersistence(false);
}

// ------------------------------------------------------ full-screen views

void World::drawSingleCubeFrame(uint32_t now) {
  const LcdTheme &t = comp_.theme();
  comp_.clearFrame(t.bezelOut);
  // 32x32 @ 7x scale = 224x224 centered with 8px recessed dark bezel
  comp_.drawRecessedBezel(8, 8, 224, 224, 8);
  renderCubeToLcd(0, lcd_[0], now, false);
  comp_.renderLcd32(lcd_[0], 8, 8, 7, true);
  comp_.present();
}

void World::drawLinkedWorldFrame(uint32_t now) {
  const LcdTheme &t = comp_.theme();
  comp_.clearFrame(t.bezelOut);

  // Header strip (monochrome)
  comp_.fillRect(0, 0, 240, 20, t.bezelIn);
  comp_.drawTextScaled("OPEN CUBE WORLD", 8, 5, 2, t.offCol);
  const char *st = "LIVE";
  if (session == SESS_BOOT) st = "BOOT";
  else if (session == SESS_HELLO) st = "HELLO";
  else if (session == SESS_BYE) st = "BYE";
  else if (session == SESS_END) st = "END";
  comp_.drawTextScaled(st, 196, 5, 2, t.offCol);

  // Two side-by-side 32x32 monochrome LCDs @ 3x scale = 96x96 each
  const int sc = 3;
  const int sz = 32 * sc;  // 96
  const int y0 = 38;
  const int x0 = 14;
  const int x1 = 130;

  comp_.drawRecessedBezel(x0, y0, sz, sz, 5);
  comp_.drawRecessedBezel(x1, y0, sz, sz, 5);

  // Magnetic link indicator between the two cubes
  bool pulse = ((now / 350) & 1) != 0;
  comp_.fillRect(x0 + sz + 5, y0 + sz / 2 - 4, x1 - (x0 + sz) - 10, 8, pulse ? t.offCol : t.gridCol);

  renderCubeToLcd(0, lcd_[0], now, false);
  renderCubeToLcd(1, lcd_[1], now, visiting_);
  comp_.renderLcd32(lcd_[0], x0, y0, sc, false);
  comp_.renderLcd32(lcd_[1], x1, y0, sc, false);

  // Character labels under each cube
  comp_.drawTextScaled(kCharNames[cubes_[0].charIdx], x0 + 24, y0 + sz + 10, 2, t.offCol);
  comp_.drawTextScaled(kCharNames[cubes_[1].charIdx], x1 + 18, y0 + sz + 10, 2, t.offCol);

  // Bottom monochrome protocol monitor
  comp_.fillRect(8, 168, 224, 64, t.bezelIn);
  comp_.fillRect(10, 170, 220, 60, t.offCol);
  if (hasLastPacket) {
    char line1[32];
    snprintf(line1, sizeof(line1), "%s %X%X%X%X%X%X%X",
             CubeProtocol::typeName(lastPacketCopy[0]),
             lastPacketCopy[0], lastPacketCopy[1], lastPacketCopy[2],
             lastPacketCopy[3], lastPacketCopy[4], lastPacketCopy[5],
             lastPacketCopy[6]);
    comp_.drawTextScaled(line1, 16, 178, 2, t.onCol);
    char line2[32];
    snprintf(line2, sizeof(line2), "ID:%02X  TRL:%X  %lus",
             lastPacketCopy[5], lastPacketCopy[6],
             (unsigned long)((now - lastPacketAt) / 1000));
    comp_.drawTextScaled(line2, 16, 198, 2, t.onCol);
    comp_.drawTextScaled("BTN2:WHIP  BTN1:CHAR", 16, 216, 1, t.onCol);
  } else {
    comp_.drawTextScaled("PROTOCOL MONITOR", 16, 178, 2, t.onCol);
    comp_.drawTextScaled("1-WIRE 3.3V 7-NIBBLE", 16, 198, 2, t.onCol);
    comp_.drawTextScaled("BTN2:WHIP  BTN3:MODE", 16, 216, 1, t.onCol);
  }

  comp_.present();
}

void World::drawDebugFrame(uint32_t now) {
  const LcdTheme &t = comp_.theme();
  comp_.clearFrame(t.bezelOut);
  comp_.drawRecessedBezel(8, 8, 224, 224, 8);

  Lcd32 &d = lcd_[0];
  d.clear(false);

  if (debugPage_ == 0) {
    // Authentic Radica Debug Mode: border dots + live QMI8658 IMU tilt box + char/palette
    for (int i = 0; i < 32; i += 4) {
      d.setPx(i, 0, true); d.setPx(i, 31, true);
      d.setPx(0, i, true); d.setPx(31, i, true);
    }
    d.drawText("OCW DBG", 2, 2, true);
    d.drawText(kCharNames[cubes_[0].charIdx], 2, 9, true);
    d.drawText(imu_.ok ? "IMU OK" : "NO IMU", 2, 16, true);
    d.drawText(comp_.theme().name, 2, 24, true);

    // Live tilt square indicator (like Radica debug_directions.gif!)
    int cx = 23 + constrain((int)(imu_.tiltX * 5.0f), -4, 4);
    int cy = 22 + constrain((int)(imu_.ay * 5.0f), -4, 4);
    d.fillRect(19, 18, 10, 10, false);
    d.hline(19, 28, 18, true); d.hline(19, 28, 27, true);
    d.vline(19, 18, 27, true); d.vline(28, 18, 27, true);
    d.fillRect(cx - 1, cy - 1, 3, 3, true);
  } else if (debugPage_ >= 1 && debugPage_ <= 4) {
    // Authentic 32x32 LCD pixel tests (H-stripes, V-stripes, checker, all-on)
    d.drawLcdTestPattern((uint8_t)(debugPage_ - 1));
  } else {
    // Radica copyright-style + protocol info card
    d.drawText("32X32", 5, 3, true);
    d.drawText("1-BIT", 5, 10, true);
    d.drawText("7-NIB", 5, 17, true);
    d.drawText("TRL 9", 5, 24, true);
  }

  (void)now;
  d.stepPersistence(true);
  comp_.renderLcd32(d, 8, 8, 7, true);
  comp_.present();
}

void World::draw() {
  uint32_t now = millis();
  switch (view_) {
    case VIEW_LINKED_WORLD: drawLinkedWorldFrame(now); break;
    case VIEW_DEBUG:        drawDebugFrame(now); break;
    default:                drawSingleCubeFrame(now); break;
  }
}

void World::drawBoot(uint32_t now) {
  bootDone_ = false;
  bootStart_ = now - ((now / 15) % 1500);
  drawSingleCubeFrame(now);
}

void World::drawEnd(uint32_t now) {
  (void)now;
  draw();
}

}  // namespace ocw
