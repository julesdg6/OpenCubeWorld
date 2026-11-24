/*
 * OpenCubeWorld.ino - main application.
 *
 * A self-contained Cube World "world" on the Waveshare 1.54inch Touch LCD
 * Module (SKU 34370): cubes dance to a simulated Cube World protocol
 * session (HELLO -> keepalives -> whips -> BYE -> END), with a live
 * protocol monitor at the bottom of the screen.
 *
 * Touch controls:
 *   - tap the play area        : whip the cube you tapped
 *   - tap top-left corner      : cycle 2/3/4 cubes (restarts session)
 *   - tap top-right corner     : protocol info screen
 *
 * See README.md, docs/PROTOCOL.md and the other sketches (Replay_*,
 * CubeLink) in this repo.
 */
#include <OpenCubeWorld.h>

static Arduino_GFX *gfx;
static ocw::World world(nullptr);

// --- live session simulation state ---------------------------------------
static uint32_t nextKeepAt = 0;
static int keepIdx = 0;
static uint32_t nextWhipAt = 0;
static uint32_t helloAt = 0;
static int helloStep = 0;
static uint32_t sessionStart = 0;
static bool byeSent = false, endSent = false;
static uint32_t lastTouch = 0;
static bool infoScreen = false;

static void monitor(const uint8_t p[7]) {
  world.lastPacket = p;
  world.lastPacketAt = millis();
}

static void simPacket(uint8_t p[7]) {
  monitor(p);
  switch (CubeProtocol::type(p)) {
    case CW_TYPE_HELLO:     world.onHello(p[5]); break;
    case CW_TYPE_KEEPALIVE: world.onKeepalive(p[1], p[5]); break;
    case CW_TYPE_BYE:       world.onBye(p[5]); break;
    case CW_TYPE_END:       world.onEnd(p[5]); break;
    default: break;
  }
}

// The red "whip" cube when it is in the session, else cube 0.
static int whipSender() {
  int s = world.findCube(ocw::kCharDefaultIds[ocw::CHAR_WHIP]);
  return (s >= 0) ? s : 0;
}

static void doWhip(int sender, int target) {
  uint8_t p[7];
  CubeProtocol::mkAction(random(2) + 1,   // a1 in {1,2} (observed)
                         random(2) * 2,   // a2 in {0,2} (observed)
                         random(12) + 2,  // a3 in {2..0xf} (observed range)
                         ocw::kCharDefaultIds[sender], p);
  monitor(p);
  world.onWhipIdx(sender, target);
}

static void startSession() {
  world.session = ocw::SESS_HELLO;
  helloStep = 0;
  helloAt = millis() + 200;
  nextKeepAt = 0;
  nextWhipAt = millis() + 2000;
  sessionStart = millis();
  byeSent = endSent = false;
}

void setup() {
  Serial.begin(115200);
  gfx = ocwCreateDisplay();
  gfx->begin();
  ocwBacklightOn();
  cst816d_init(OCW_TP_RST_PIN, -1);  // polled, no interrupt
  world = ocw::World(gfx);
  world.begin();
  world.setup(3);
  randomSeed(analogRead(A0));
  Serial.println("OpenCubeWorld ready");
}

static bool touchEvent(int16_t &x, int16_t &y) {
  touch_data_t td;
  if (!get_touch_data(&td) || td.touch_num == 0) return false;
  x = td.coords[0].x;
  y = td.coords[0].y;
  uint32_t now = millis();
  if (now - lastTouch < 350) return false;
  lastTouch = now;
  return true;
}

static void drawInfo() {
  gfx->fillScreen(ocw::COL_BG);
  gfx->fillRect(0, 0, 240, 14, ocw::COL_BAR);
  gfx->setTextColor(ocw::COL_TEXT);
  gfx->setCursor(4, 3);
  gfx->print("PROTOCOL");
  const char *lines[] = {
    "type meaning        shape",
    " 1   HELLO          1 f 1 5 1 id 9",
    " 2   KEEPALIVE      2 s 0 0 0 id 9",
    " 6   WHIP           6 a b c 0 id 9",
    " 7   BYE            7 0 0 0 0 id 9",
    " 8   END            8 0 0 0 0 id 9",
    " 9   unknown        9 0 0 0 0 7 9",
    "",
    "trailer always 9 (559/559)",
    "28 bits: L94=0 L174=1",
    "pre L234 H53 L94  post H57 L93",
  };
  for (int i = 0; i < 11; i++) {
    gfx->setTextColor(i == 0 ? ocw::COL_DIM : ocw::COL_TEXT);
    gfx->setCursor(4, 20 + i * 14);
    gfx->print(lines[i]);
  }
  gfx->setTextColor(ocw::COL_ACCENT);
  gfx->setCursor(84, 228);
  gfx->print("touch to go back");
}

void loop() {
  if (infoScreen) {
    drawInfo();
    int16_t tx, ty;
    if (touchEvent(tx, ty)) infoScreen = false;
    return;
  }

  uint32_t now = millis();
  int16_t tx, ty;

  if (world.session == ocw::SESS_BOOT) {
    world.drawBoot(now);
    if (touchEvent(tx, ty)) startSession();
    return;
  }
  if (world.session == ocw::SESS_END) {
    world.drawEnd(now);
    if (touchEvent(tx, ty)) world.resetToBoot();
    return;
  }

  // ---- HELLO phase: each cube announces in turn --------------------------
  if (world.session == ocw::SESS_HELLO && now >= helloAt && helloStep < world.nCubes) {
    uint8_t p[7];
    CubeProtocol::mkHello(ocw::kCharDefaultIds[helloStep], p);
    simPacket(p);
    helloStep++;
    helloAt = now + 600;
  }

  // ---- ACTIVE: keepalives round-robin, random whips, auto-end ------------
  if (world.session == ocw::SESS_ACTIVE) {
    if (now >= nextKeepAt) {
      uint8_t p[7];
      int ci = keepIdx % world.nCubes;
      CubeProtocol::mkKeepalive(ci, ocw::kCharDefaultIds[ci], p);
      simPacket(p);
      keepIdx++;
      nextKeepAt = now + 350;
    }
    if (now >= nextWhipAt) {
      int s = whipSender();
      int t = random(1, world.nCubes);
      if (t == s) t = (t + 1) % world.nCubes;
      doWhip(s, t);
      nextWhipAt = now + 3500 + random(4500);
    }
    // after 45 s of play: BYE, then END
    if (!byeSent && now > sessionStart + 45000) {
      uint8_t p[7];
      CubeProtocol::mkBye(ocw::kCharDefaultIds[0], p);
      simPacket(p);
      byeSent = true;
    }
    if (byeSent && !endSent && now > sessionStart + 46500) {
      uint8_t p[7];
      CubeProtocol::mkEnd(ocw::kCharDefaultIds[0], p);
      simPacket(p);
      endSent = true;
    }
  }

  // ---- touch: whip / menu / info -----------------------------------------
  if (touchEvent(tx, ty)) {
    if (ty < 14) {
      if (tx < 60) {
        int n = world.nCubes + 1;
        if (n > 4) n = 2;
        world.resetToBoot();
        world.setup(n);
      } else if (tx > 180) {
        infoScreen = true;
      }
      return;
    }
    if (world.session == ocw::SESS_ACTIVE) {
      int t = world.nearestCube(tx, ty);
      int s = whipSender();
      if (t >= 0 && t != s) doWhip(s, t);
    }
  }

  world.update(now);
  world.draw();
  delay(50);  // ~20 fps; full-screen redraw
}
