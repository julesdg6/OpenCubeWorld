/*
 * world.h - OpenCubeWorld display engine for the Waveshare 1.54inch
 * Touch LCD (240x240, ST7789S).
 *
 * Renders a "world" of dancing cubes and reacts to Cube World protocol
 * events (from a live simulation, a capture replay, or a real hardware
 * link). All timing is driven by micros()/millis() passed in from the
 * sketch, so it is unit-testable and board-agnostic.
 */
#pragma once
#include <Arduino.h>
#include "sprites.h"

class Arduino_GFX;

namespace ocw {

// Screen colours (RGB565)
enum {
  COL_BG      = 0x0841, // near-black blue
  COL_BG2     = 0x0A49, // panel
  COL_GROUND  = 0x3E0F, // dark green
  COL_GRASS   = 0x2E8F,
  COL_BAR     = 0x1298, // top/bottom bar
  COL_TEXT    = 0xE73F, // off-white
  COL_DIM     = 0x64A2,
  COL_ACCENT  = 0xF640, // yellow
  COL_RED     = 0xF800,
  COL_GREEN   = 0x3EF0,
  COL_BLUE    = 0x07E0,
};

// Session states (the toy's observed session lifecycle)
enum Session {
  SESS_BOOT = 0,  // title screen, waiting for touch
  SESS_HELLO,     // cubes introducing themselves (HELLO packets)
  SESS_ACTIVE,    // keepalives + whips
  SESS_BYE,       // waving goodbye (BYE packets)
  SESS_END,       // seated (END packet), waiting for touch
};

// Per-cube animation states
enum Anim {
  ANIM_IDLE = 0,
  ANIM_WHIP,
  ANIM_DODGE,
  ANIM_WAVE,
  ANIM_SIT,
};

// Character indices (must match sprites.h ordering)
enum CharIdx {
  CHAR_OPEN = 0,
  CHAR_WHIP,
  CHAR_DODGER,
  CHAR_HANS,
  CHAR_HANDY,
  N_CHARS
};

// Character names used in the protocol (for the monitor / docs)
extern const char *const kCharNames[N_CHARS];
// Default cube protocol ids (chosen from ids observed in the captures)
extern const uint8_t kCharDefaultIds[N_CHARS];

class World {
 public:
  static const int MAX_CUBES = 4;

  struct Cube {
    int charIdx = 0;
    uint8_t id = 0;
    uint8_t slot = 0;
    int16_t x = 0, y = 0;   // top-left at current scale
    int scale = 4;
    uint8_t anim = ANIM_IDLE;
    uint32_t animUntil = 0;
    uint32_t hopUntil = 0;      // keepalive hop (y bump)
    uint32_t lastIdleFlip = 0;
    bool present = false;
    int16_t dx = 0, dy = 0;     // transient offset (dodge lean)
  };

  explicit World(Arduino_GFX *gfx);

  void begin();
  void setup(int nCubes);          // place cubes for a session
  void resetToBoot();

  // Protocol event entry points (any source: sim / replay / hardware)
  void onHello(uint8_t id);
  void onKeepalive(uint8_t slot, uint8_t id);
  void onWhip(uint8_t a1, uint8_t a2, uint8_t a3, uint8_t senderId,
              int16_t targetX, int16_t targetY);  // target = nearest cube
  void onBye(uint8_t id);
  void onEnd(uint8_t id);

  // Find the cube with a given protocol id (-1 if none).
  int findCube(uint8_t id) const;
  // Re-identify a cube once its real protocol id is known (hardware mode).
  void adoptCube(int idx, uint8_t id) {
    if (idx >= 0 && idx < MAX_CUBES && cubes_[idx].present) cubes_[idx].id = id;
  }
  // Whip animation with known cube indices (live mode / hardware mode).
  void onWhipIdx(int senderIdx, int targetIdx);
  // Nearest present cube to a screen point (for touch targeting).
  int nearestCube(int16_t x, int16_t y) const;

  void update(uint32_t now_ms);
  void draw();

  Session session = SESS_BOOT;
  uint32_t sessionUntil = 0;
  int nCubes = 3;
  bool showFx = true;

  // Protocol monitor
  const uint8_t *lastPacket = nullptr;
  uint32_t lastPacketAt = 0;

  // Title / end screens
  void drawBoot(uint32_t now_ms);
  void drawEnd(uint32_t now_ms);

 private:
  Arduino_GFX *gfx_ = nullptr;
  Cube cubes_[MAX_CUBES];

  // whip effect state
  uint32_t whipFxUntil = 0;
  int16_t whipFxX0 = 0, whipFxY0 = 0, whipFxX1 = 0, whipFxY1 = 0;
  uint32_t dustUntil = 0;
  int16_t dustX = 0, dustY = 0;

  uint32_t lastFullRedraw = 0;

  void drawBar(uint32_t now_ms);
  void drawGround();
  void drawCube(const Cube &c, uint32_t now_ms);
  void drawWhipFx(uint32_t now_ms);
  void drawMonitor();
  void drawSpeech(int idx, const char *text, uint32_t until);
  uint32_t speechUntil_[MAX_CUBES] = {0, 0, 0, 0};
  char speech_[MAX_CUBES][8] = {{0}, {0}, {0}, {0}};
  void say(int idx, const char *text, uint32_t ms);
  void drawSpriteAt(const uint8_t frame[128], int16_t x, int16_t y, int scale);
};

}  // namespace ocw
