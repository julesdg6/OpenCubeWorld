/*
 * CubeLink.ino - hardware mode: join a real Radica Cube World session.
 *
 * Wires the Waveshare 1.54inch Touch LCD Module to a Cube World cube's
 * single data line (see docs/HARDWARE.md for the 3.3V level notes) and
 * plays along using the real protocol:
 *
 *   - polls for a peer cube with the attention pulse (L234/H53/L94),
 *     exactly like the original radica-cube-world-hack .ino;
 *   - answers HELLO with its own HELLO (our id = 0x01, like the
 *     original Arduino cube);
 *   - keeps the session alive with round-robin KEEPALIVE packets on
 *     slot 0 (the slot the toys most often use for the first speaker);
 *   - sends WHIP action packets when you tap the screen (action
 *     nibbles within the observed ranges);
 *   - answers BYE with BYE + END and starts searching again.
 *
 * The screen shows our cube (green "open") plus the peer (drawn as the
 * blue "dodger" until its real id is known, then re-identified), the
 * usual animations, and a live protocol monitor for every packet
 * in either direction.
 */
#include <OpenCubeWorld.h>

static Arduino_GFX *gfx;
static ocw::World world(nullptr);
static CubeProtocol proto(OCW_CUBE_DATA_PIN);

static const uint8_t OUR_ID = 0x01;  // same id as the original Arduino cube

enum LinkState { LINK_SEARCH, LINK_HELLO, LINK_ACTIVE, LINK_END };
static LinkState link = LINK_SEARCH;
static uint8_t peerId = 0;
static bool peerSeen = false;
static uint32_t nextKeepAt = 0;
static uint32_t nextPollAt = 0;
static uint32_t lastTouch = 0;
static uint32_t endAt = 0;

static void monitor(const uint8_t p[7]) {
  world.lastPacket = p;
  world.lastPacketAt = millis();
}

static void handlePacket(const uint8_t p[7]) {
  monitor(p);
  uint8_t t = CubeProtocol::type(p);
  uint8_t id = CubeProtocol::id(p);

  if (!peerSeen && t != CW_TYPE_END) {
    peerSeen = true;
    peerId = id;
    world.adoptCube(1, peerId);
  }

  switch (t) {
    case CW_TYPE_HELLO:
      world.onHello(id);
      if (link == LINK_HELLO || link == LINK_ACTIVE) {
        // answer with our HELLO (the toys do this on meeting)
        uint8_t mine[7];
        CubeProtocol::mkHello(OUR_ID, mine);
        proto.sendPacket(mine);
        monitor(mine);
        link = LINK_ACTIVE;
      }
      break;
    case CW_TYPE_KEEPALIVE:
      world.onKeepalive(CubeProtocol::slot(p), id);
      break;
    case CW_TYPE_ACTION:
      // the peer whipped us (or its partner): dodge + counter-whip visual
      world.onWhip(p[1], p[2], p[3], id, 60, 140);
      break;
    case CW_TYPE_BYE:
      world.onBye(id);
      link = LINK_END;
      endAt = millis();
      break;
    case CW_TYPE_END:
      world.onEnd(id);
      link = LINK_END;
      endAt = millis();
      break;
    default:
      break;
  }
}

static void doWhip() {
  uint8_t p[7];
  CubeProtocol::mkAction(random(2) + 1, random(2) * 2, random(12) + 2, OUR_ID, p);
  proto.sendPacket(p);
  monitor(p);
  world.onWhipIdx(0, 1);  // we whip the peer
}

void setup() {
  Serial.begin(115200);
  gfx = ocwCreateDisplay();
  gfx->begin();
  ocwBacklightOn();
  cst816d_init(OCW_TP_RST_PIN, -1);
  world = ocw::World(gfx);
  world.begin();
  world.setup(2);
  world.session = ocw::SESS_ACTIVE;
  randomSeed(analogRead(A0));
  Serial.println("OpenCubeWorld CubeLink: searching for a cube...");
}

static bool touchEvent() {
  touch_data_t td;
  if (!get_touch_data(&td) || td.touch_num == 0) return false;
  uint32_t now = millis();
  if (now - lastTouch < 350) return false;
  lastTouch = now;
  return true;
}

static void drawStatus() {
  const char *s;
  switch (link) {
    case LINK_SEARCH: s = "SEARCH"; break;
    case LINK_HELLO: s = "HELLO"; break;
    case LINK_ACTIVE: s = "LINKED"; break;
    default: s = "END"; break;
  }
  gfx->setTextColor(ocw::COL_GREEN);
  gfx->setCursor(178, 3);
  gfx->print(s);
}

void loop() {
  uint32_t now = millis();

  switch (link) {
    case LINK_SEARCH:
      if (now >= nextPollAt) {
        if (proto.pollForPeer(1500)) {
          link = LINK_HELLO;
          Serial.println("peer found, waiting for HELLO");
        }
        nextPollAt = now + 400;  // cadence of the original .ino (delay 350)
      }
      break;
    default:
      break;
  }

  // continuous listener in HELLO/ACTIVE
  if (link == LINK_HELLO || link == LINK_ACTIVE) {
    uint8_t p[7];
    if (proto.readPacket(p, 40000)) {
      handlePacket(p);
    }
    if (link == LINK_ACTIVE && now >= nextKeepAt) {
      uint8_t mine[7];
      CubeProtocol::mkKeepalive(0, OUR_ID, mine);
      proto.sendPacket(mine);
      monitor(mine);
      nextKeepAt = now + 1400;
    }
  }

  if (link == LINK_END) {
    // answer the end of the session, then start searching again
    static uint8_t answered = 0;
    if (!answered) {
      uint8_t p[7];
      CubeProtocol::mkBye(OUR_ID, p);
      proto.sendPacket(p);
      monitor(p);
      delay(150);
      CubeProtocol::mkEnd(OUR_ID, p);
      proto.sendPacket(p);
      monitor(p);
      answered = 1;
    }
    if (now > endAt + 3000) {
      link = LINK_SEARCH;
      peerSeen = false;
      answered = 0;
      world.session = ocw::SESS_ACTIVE;
      Serial.println("back to searching");
    }
  }

  if (touchEvent() && link == LINK_ACTIVE) doWhip();

  world.update(now);
  world.draw();
  drawStatus();
  delay(40);
}
