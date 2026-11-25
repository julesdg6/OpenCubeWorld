/*
 * world.h - Authentic 32x32 1-bit monochrome Radica Cube World engine.
 *
 * Supports three interactive modes on the 240x240 ST7789 display:
 *   - VIEW_SINGLE_CUBE : Authentic full-screen 32x32 monochrome LCD stickman
 *                        (7x scale = 224x224 + 8px recessed bezel) with power-on
 *                        maze wipe, autonomous animations, signature props, and
 *                        real QMI8658 IMU tilt/flip/shake reactions.
 *   - VIEW_LINKED_WORLD: Side-by-side linked monochrome 32x32 mini-cubes with
 *                        magnetic transfer (Venetian blind), whip/dodge, and
 *                        live Cube World protocol packet monitor.
 *   - VIEW_DEBUG       : Authentic Radica debug mode (IMU crosshair, 32x32 LCD
 *                        stripe tests, and protocol inspector).
 */
#pragma once
#include <Arduino.h>
#include "sprites.h"
#include "lcd32.h"
#include "qmi8658.h"

namespace ocw {

// Monochrome-friendly compatibility colors (used by sketches/overlays)
enum {
  COL_BG     = 0x0841,
  COL_BAR    = 0x18E2,
  COL_TEXT   = 0xBE55,
  COL_DIM    = 0x6B4D,
  COL_ACCENT = 0xDEFB,
  COL_GREEN  = 0xBE55,
};

enum ViewMode {
  VIEW_SINGLE_CUBE  = 0,
  VIEW_LINKED_WORLD = 1,
  VIEW_DEBUG        = 2,
  VIEW_N_MODES      = 3,
};

enum Session {
  SESS_BOOT = 0,
  SESS_HELLO,
  SESS_ACTIVE,
  SESS_BYE,
  SESS_END,
};

enum Anim {
  ANIM_IDLE = 0,
  ANIM_WALK,
  ANIM_STRETCH,
  ANIM_SIT,
  ANIM_ITCH,
  ANIM_MAD,
  ANIM_KICK,
  ANIM_TRICK,
  ANIM_WHIP,
  ANIM_DODGE,
  ANIM_WAVE,
  ANIM_TUMBLE,
  ANIM_DIZZY,
  ANIM_SLEEP,
};

enum CharIdx {
  CHAR_WHIP = 0,    // 0x06 - Series 1: rope / whip
  CHAR_DODGER,      // 0x0E - Series 1: kicking ball
  CHAR_HANS,        // 0x0D - Series 2: barbell / fitness
  CHAR_HANDY,       // 0x0B - Series 2: wrench / handyman
  CHAR_OPEN,        // 0x01 - Series 1: Scoop / Open (dog Spot)
  N_CHARS
};

extern const char *const kCharNames[N_CHARS];
extern const uint8_t kCharDefaultIds[N_CHARS];

class World {
 public:
  static const int MAX_CUBES = 2;

  struct Cube {
    int charIdx = CHAR_WHIP;
    uint8_t id = 0x06;
    uint8_t slot = 0;
    int8_t posX = 10;          // horizontal offset inside 32x32 LCD
    int8_t walkDir = 1;        // +1 right, -1 left
    bool facingLeft = false;
    uint8_t anim = ANIM_IDLE;
    uint32_t animStart = 0;
    uint32_t animUntil = 0;
    uint32_t hopUntil = 0;
    bool present = true;
    bool vacated = false;      // true while visiting neighbor (shows Venetian blind)
    uint8_t blindRows = 0;
  };

  explicit World(Arduino_GFX *gfx);
  ~World();

  void begin();
  void setup(int nCubes);
  void resetToBoot();

  // Interactive controls (buttons / touch / serial)
  void setViewMode(ViewMode mode);
  ViewMode viewMode() const { return view_; }
  void cycleViewMode();
  void cycleCharacter();
  void cyclePalette();
  void triggerAction();
  void triggerFlip();
  void triggerShake();
  void triggerTransfer();
  void wakeUp();
  void cycleDebugPage();

  // Feed live QMI8658 IMU state
  void setImu(const ImuState &imu);

  // Protocol event entry points (sim / replay / hardware link)
  void onHello(uint8_t id);
  void onKeepalive(uint8_t slot, uint8_t id);
  void onWhip(uint8_t a1, uint8_t a2, uint8_t a3, uint8_t senderId,
              int16_t targetX, int16_t targetY);
  void onWhipIdx(int senderIdx, int targetIdx);
  void onBye(uint8_t id);
  void onEnd(uint8_t id);

  int findCube(uint8_t id) const;
  void adoptCube(int idx, uint8_t id) {
    if (idx >= 0 && idx < MAX_CUBES) cubes_[idx].id = id;
  }
  int nearestCube(int16_t x, int16_t y) const;

  void update(uint32_t now_ms);
  void draw();
  void drawBoot(uint32_t now_ms);
  void drawEnd(uint32_t now_ms);

  // State fields kept public for sketches (OpenCubeWorld, CubeLink, Replay_*)
  Session session = SESS_BOOT;
  int nCubes = 2;
  bool showFx = true;
  const uint8_t *lastPacket = nullptr;
  uint32_t lastPacketAt = 0;
  uint8_t lastPacketCopy[7] = {0};
  bool hasLastPacket = false;

  const char *activeCharName() const { return kCharNames[cubes_[0].charIdx]; }
  const char *paletteName() const { return comp_.theme().name; }
  uint8_t debugPage() const { return debugPage_; }

 private:
  Arduino_GFX *gfx_ = nullptr;
  LcdCompositor comp_;
  Lcd32 lcd_[MAX_CUBES];
  Cube cubes_[MAX_CUBES];
  ImuState imu_;

  ViewMode view_ = VIEW_SINGLE_CUBE;
  uint32_t bootStart_ = 0;
  bool bootDone_ = false;
  uint32_t lastActivity_ = 0;
  uint32_t nextAutoAnim_ = 0;
  uint32_t lastRenderStep_ = 0;

  // Brief monochrome pixel toast (e.g. "WHIP", "STN", "B&W")
  char toast_[10] = {0};
  uint32_t toastUntil_ = 0;

  // Per-cube pixel speech tag
  char speech_[MAX_CUBES][6] = {{0}, {0}};
  uint32_t speechUntil_[MAX_CUBES] = {0, 0};

  // Linked-cube transfer state (Cube 0 visits Cube 1 -> Venetian blind on Cube 0)
  bool visiting_ = false;
  uint32_t visitUntil_ = 0;

  // Debug page (0=IMU+status, 1..4=LCD test patterns, 5=protocol info)
  uint8_t debugPage_ = 0;

  void showToast(const char *msg, uint32_t ms = 1100);
  void say(int idx, const char *msg, uint32_t ms = 900);
  void setAnim(int idx, Anim a, uint32_t durMs, uint32_t now);
  void renderCubeToLcd(int idx, Lcd32 &lcd, uint32_t now, bool includeVisitor);
  void drawStickmanOnLcd(const Cube &c, Lcd32 &lcd, uint32_t now, int xOverride = -99);
  void drawSingleCubeFrame(uint32_t now);
  void drawLinkedWorldFrame(uint32_t now);
  void drawDebugFrame(uint32_t now);
};

}  // namespace ocw
