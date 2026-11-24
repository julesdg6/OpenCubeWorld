/*
 * world.cpp - OpenCubeWorld display engine. See world.h.
 */
#include "world.h"
#include "cubeprotocol.h"
#include <Arduino_GFX_Library.h>

namespace ocw {

// PROGMEM-safe byte read (AVR keeps const tables in flash)
static uint8_t ocwReadByte(const uint8_t *p, int i) {
#if defined(__AVR__)
  return pgm_read_byte(p + i);
#else
  return p[i];
#endif
}

const char *const kCharNames[N_CHARS] = {
  "open", "whip", "dodger", "hans", "handy",
};

// Ids chosen from the set observed in the three captures:
// 0x01 (the original Arduino cube), 0x0e/0x0d/0x0b (keepalive senders),
// 0x06 (a whip action sender).
const uint8_t kCharDefaultIds[N_CHARS] = {0x01, 0x06, 0x0E, 0x0D, 0x0B};

// Sprite tables (one per character, in CharIdx order), flattened:
// frame N lives at kCharFrames[i] + N * 128.
static const uint8_t *const kCharFrames[N_CHARS] = {
  (const uint8_t *)ocw_open, (const uint8_t *)ocw_whip, (const uint8_t *)ocw_dodger,
  (const uint8_t *)ocw_hans, (const uint8_t *)ocw_handy,
};

World::World(Arduino_GFX *gfx) : gfx_(gfx) {}

void World::begin() {
  gfx_->setRotation(0);
  gfx_->fillScreen(COL_BG);
  setup(3);
  session = SESS_BOOT;
}

void World::setup(int nCubes) {
  nCubes = constrain(nCubes, 2, MAX_CUBES);
  // Character roster: the "open" cube first, then whipper + dancers.
  static const int roster[4] = {CHAR_OPEN, CHAR_WHIP, CHAR_DODGER, CHAR_HANDY};
  int scale = (nCubes <= 3) ? 4 : 3;
  int w = 16 * scale;
  int gap = (nCubes <= 3) ? 16 : 4;
  int total = nCubes * w + (nCubes - 1) * gap;
  int x0 = (240 - total) / 2;
  int groundY = 196;
  for (int i = 0; i < MAX_CUBES; i++) {
    Cube &c = cubes_[i];
    if (i < nCubes) {
      c.present = true;
      c.charIdx = roster[i];
      c.id = kCharDefaultIds[roster[i]];
      c.slot = i;
      c.scale = scale;
      c.x = x0 + i * (w + gap);
      c.y = groundY - w;
      c.anim = ANIM_IDLE;
      c.animUntil = 0;
      c.hopUntil = 0;
      c.dx = c.dy = 0;
    } else {
      c.present = false;
    }
    speechUntil_[i] = 0;
    speech_[i][0] = 0;
  }
  lastPacket = nullptr;
  whipFxUntil = 0;
  dustUntil = 0;
}

void World::resetToBoot() {
  session = SESS_BOOT;
  sessionUntil = 0;
  setup(nCubes);
}

int World::findCube(uint8_t id) const {
  for (int i = 0; i < MAX_CUBES; i++)
    if (cubes_[i].present && cubes_[i].id == id) return i;
  return -1;
}

int World::nearestCube(int16_t x, int16_t y) const {
  int best = -1;
  int bestD = 1 << 30;
  for (int i = 0; i < MAX_CUBES; i++) {
    const Cube &c = cubes_[i];
    if (!c.present) continue;
    int cx = c.x + 8 * c.scale, cy = c.y + 8 * c.scale;
    int d = abs(cx - x) + abs(cy - y);
    if (d < bestD) { bestD = d; best = i; }
  }
  return best;
}

void World::say(int idx, const char *text, uint32_t ms) {
  strncpy(speech_[idx], text, sizeof(speech_[idx]) - 1);
  speech_[idx][sizeof(speech_[idx]) - 1] = 0;
  speechUntil_[idx] = millis() + ms;
}

// ------------------------------------------------------------------ events

void World::onHello(uint8_t id) {
  if (session == SESS_BOOT) session = SESS_HELLO;
  int i = findCube(id);
  if (i >= 0) say(i, "HI!", 1200);
}

void World::onKeepalive(uint8_t slot, uint8_t id) {
  (void)slot;
  if (session == SESS_HELLO) session = SESS_ACTIVE;
  int i = findCube(id);
  if (i >= 0) {
    cubes_[i].hopUntil = millis() + 180;
  }
}

void World::onWhip(uint8_t a1, uint8_t a2, uint8_t a3, uint8_t senderId,
                   int16_t targetX, int16_t targetY) {
  (void)a1; (void)a2; (void)a3;
  if (session == SESS_HELLO) session = SESS_ACTIVE;
  int s = findCube(senderId);
  if (s < 0) s = (nCubes > 1) ? 1 : 0;   // fall back to the whipper slot
  int t = nearestCube(targetX, targetY);
  if (t < 0 || t == s) t = (s + 1) % nCubes;
  onWhipIdx(s, t);
}

void World::onWhipIdx(int senderIdx, int targetIdx) {
  if (session == SESS_HELLO) session = SESS_ACTIVE;
  uint32_t now = millis();
  int s = senderIdx, t = targetIdx;
  if (s < 0 || s >= nCubes || !cubes_[s].present) s = 0;
  if (t < 0 || t >= nCubes || !cubes_[t].present || t == s) t = (s + 1) % nCubes;
  if (t == s) return;

  cubes_[s].anim = ANIM_WHIP;
  cubes_[s].animUntil = now + 600;
  cubes_[t].anim = ANIM_DODGE;
  cubes_[t].animUntil = now + 600;
  cubes_[t].dx = (cubes_[t].x > cubes_[s].x) ? 6 : -6;
  cubes_[t].dy = 2;

  const Cube &cs = cubes_[s], &ct = cubes_[t];
  whipFxX0 = cs.x + 8 * cs.scale;
  whipFxY0 = cs.y + 4 * cs.scale;
  whipFxX1 = ct.x + 8 * ct.scale;
  whipFxY1 = ct.y + 8 * ct.scale;
  whipFxUntil = now + 420;
  dustX = ct.x + 2 * ct.scale;
  dustY = ct.y + 12 * ct.scale;
  dustUntil = now + 300;
  say(t, "!!", 700);
}

void World::onBye(uint8_t id) {
  session = SESS_BYE;
  int i = findCube(id);
  if (i >= 0) {
    cubes_[i].anim = ANIM_WAVE;
    cubes_[i].animUntil = millis() + 1500;
    say(i, "BYE", 1500);
  }
}

void World::onEnd(uint8_t id) {
  (void)id;
  session = SESS_END;
  for (int i = 0; i < MAX_CUBES; i++)
    if (cubes_[i].present) {
      cubes_[i].anim = ANIM_SIT;
      cubes_[i].animUntil = 0;
    }
}

// ------------------------------------------------------------------- update

void World::update(uint32_t now_ms) {
  (void)now_ms;
  for (int i = 0; i < MAX_CUBES; i++) {
    Cube &c = cubes_[i];
    if (!c.present) continue;
    if (c.anim != ANIM_IDLE && millis() >= c.animUntil) {
      c.anim = ANIM_IDLE;
      c.dx = c.dy = 0;
    }
    // idle bob cadence: 400ms, staggered per cube
    if (millis() - c.lastIdleFlip > 400) {
      c.lastIdleFlip = millis();
    }
  }
}

// --------------------------------------------------------------------- draw

void World::drawSpriteAt(const uint8_t frame[128], int16_t x, int16_t y, int scale) {
  static uint16_t pal[16];
  static bool palInit = false;
  if (!palInit) {
    for (int i = 0; i < 16; i++) {
#if defined(__AVR__)
      pal[i] = pgm_read_word(&OCW_PALETTE[i]);
#else
      pal[i] = OCW_PALETTE[i];
#endif
    }
    palInit = true;
  }
  for (int py = 0; py < 16; py++) {
    for (int px = 0; px < 16; px++) {
      int k = (py * 16 + px) / 2;
      uint8_t idx = (px & 1) ? (ocwReadByte(frame, k) & 0x0F)
                             : (ocwReadByte(frame, k) >> 4);
      if (idx == 0) continue;
      gfx_->fillRect(x + px * scale, y + py * scale, scale, scale, pal[idx]);
    }
  }
}

void World::drawGround() {
  gfx_->fillRect(0, 196, 240, 8, COL_GROUND);
  gfx_->fillRect(0, 196, 240, 2, COL_GRASS);
}

void World::drawBar(uint32_t now_ms) {
  gfx_->fillRect(0, 0, 240, 14, COL_BAR);
  gfx_->setTextColor(COL_TEXT);
  gfx_->setCursor(4, 3);
  gfx_->print("OPEN CUBE WORLD");
  // right side: session state
  const char *st = "BOOT";
  switch (session) {
    case SESS_HELLO: st = "HELLO"; break;
    case SESS_ACTIVE: st = "LIVE"; break;
    case SESS_BYE: st = "BYE"; break;
    case SESS_END: st = "END"; break;
    default: break;
  }
  gfx_->setTextColor(COL_ACCENT);
  gfx_->setCursor(204, 3);
  gfx_->print(st);
  (void)now_ms;
}

void World::drawMonitor() {
  gfx_->fillRect(0, 208, 240, 32, COL_BAR);
  gfx_->setTextColor(COL_TEXT);
  if (lastPacket) {
    char buf[28];
    snprintf(buf, sizeof(buf), "%s %X %X %X %X %X %X %X",
             CubeProtocol::typeName(lastPacket[0]),
             lastPacket[0], lastPacket[1], lastPacket[2], lastPacket[3],
             lastPacket[4], lastPacket[5], lastPacket[6]);
    gfx_->setCursor(4, 212);
    gfx_->print(buf);
    gfx_->setTextColor(COL_DIM);
    char tbuf[24];
    snprintf(tbuf, sizeof(tbuf), "t=%lus", (unsigned long)((millis() - lastPacketAt) / 1000));
    gfx_->setCursor(4, 228);
    gfx_->print(tbuf);
  } else {
    gfx_->setCursor(4, 212);
    gfx_->print("protocol monitor");
    gfx_->setTextColor(COL_DIM);
    char cbuf[16];
    snprintf(cbuf, sizeof(cbuf), "%d cubes", nCubes);
    gfx_->setCursor(4, 228);
    gfx_->print(cbuf);
  }
}

void World::drawSpeech(int idx, const char *text, uint32_t until) {
  (void)text; (void)until;
  const Cube &c = cubes_[idx];
  if (speechUntil_[idx] < millis() || speech_[idx][0] == 0) return;
  int bx = c.x + c.scale * 8 - 14;
  int by = c.y - 14;
  if (bx < 2) bx = 2;
  if (bx > 214) bx = 214;
  if (by < 16) by = 16;
  gfx_->fillRect(bx, by, 28, 12, COL_TEXT);
  gfx_->setTextColor(COL_BG);
  gfx_->setCursor(bx + 2, by + 2);
  gfx_->print(speech_[idx]);
}

void World::drawCube(const Cube &c, uint32_t now_ms) {
  uint8_t frame;
  switch (c.anim) {
    case ANIM_WHIP: frame = OCW_FR_WHIP; break;
    case ANIM_DODGE: frame = OCW_FR_DODGE; break;
    case ANIM_WAVE: frame = OCW_FR_WAVE; break;
    case ANIM_SIT: frame = OCW_FR_SIT; break;
    default:
      frame = ((now_ms / 400) + c.slot) % 2 ? OCW_FR_IDLE1 : OCW_FR_IDLE0;
      break;
  }
  int16_t x = c.x + c.dx;
  int16_t y = c.y + c.dy;
  if (c.hopUntil > now_ms) y -= 3;
  // ground shadow
  gfx_->setFill(0);
  gfx_->setStrokeColor(COL_DIM);
  gfx_->drawEllipse(x + 8 * c.scale, c.y + 15 * c.scale, 6 * c.scale, 2 * c.scale, COL_DIM);
  drawSpriteAt(kCharFrames[c.charIdx] + frame * 128, x, y, c.scale);
}

void World::drawWhipFx(uint32_t now_ms) {
  if (whipFxUntil <= now_ms) return;
  uint32_t left = whipFxUntil - now_ms;
  uint8_t f = left > 280 ? 0 : (left > 140 ? 1 : 2);
  // move along the whipper->target line
  uint32_t prog = 420 - left;
  int16_t fx = whipFxX0 + (whipFxX1 - whipFxX0) * prog / 420;
  int16_t fy = whipFxY0 + (whipFxY1 - whipFxY0) * prog / 420;
  drawSpriteAt(ocw_fx_whip[f], fx - 24, fy - 24, 3);
  if (dustUntil > now_ms) {
    drawSpriteAt(ocw_fx_dust[left > 150 ? 0 : 1], dustX, dustY, 3);
  }
}

void World::draw() {
  uint32_t now = millis();
  gfx_->fillScreen(COL_BG);
  drawBar(now);
  if (session == SESS_END) {
    drawEnd(now);
    return;
  }
  drawGround();
  for (int i = 0; i < MAX_CUBES; i++)
    if (cubes_[i].present) drawCube(cubes_[i], now);
  if (showFx) drawWhipFx(now);
  for (int i = 0; i < MAX_CUBES; i++)
    if (cubes_[i].present) drawSpeech(i, speech_[i], speechUntil_[i]);
  drawMonitor();
}

void World::drawBoot(uint32_t now_ms) {
  gfx_->fillScreen(COL_BG);
  drawBar(now_ms);
  drawGround();
  // all cubes in a row, idle
  for (int i = 0; i < MAX_CUBES; i++)
    if (cubes_[i].present) drawCube(cubes_[i], now_ms);
  gfx_->setTextColor(COL_TEXT);
  gfx_->setTextSize(2);
  gfx_->setCursor(48, 40);
  gfx_->print("OPEN CUBE");
  gfx_->setCursor(56, 60);
  gfx_->print("WORLD");
  gfx_->setTextSize(1);
  gfx_->setTextColor(COL_DIM);
  if ((now_ms / 600) % 2) {
    gfx_->setCursor(78, 176);
    gfx_->print("touch to start");
  }
  drawMonitor();
}

void World::drawEnd(uint32_t now_ms) {
  drawGround();
  for (int i = 0; i < MAX_CUBES; i++)
    if (cubes_[i].present) drawCube(cubes_[i], now_ms);
  gfx_->setTextColor(COL_TEXT);
  gfx_->setTextSize(2);
  gfx_->setCursor(56, 44);
  gfx_->print("THANKS");
  gfx_->setTextSize(1);
  gfx_->setTextColor(COL_DIM);
  if ((now_ms / 600) % 2) {
    gfx_->setCursor(84, 176);
    gfx_->print("touch to restart");
  }
  drawMonitor();
}

}  // namespace ocw
