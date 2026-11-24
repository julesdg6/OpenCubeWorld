/*
 * cubeprotocol.h - Radica Cube World single-wire protocol codec.
 *
 * Reverse-engineered from the captures in npjg/radica-cube-world-hack
 * (sigrok-decoder + 3 protocol captures), then verified against the raw
 * sample data in the .sr files (see docs/PROTOCOL.md).
 *
 * Wire framing (microseconds, measured):
 *
 *   PREAMBLE   L 234 | H 53 | L 94
 *   28 BITS    each bit = [ H gap | L bit ]
 *                  gap: ~140 before the first bit, ~200 between nibbles,
 *                       ~100 within a nibble; quirk: ~100 between nibbles 6
 *                       and 7 (before bit 24)
 *                  bit: L ~94 = 0, L ~174 = 1   (thresholds 115 / 150)
 *   POSTAMBLE  H ~57 | L ~93
 *
 * A packet carries 7 nibbles (28 bits, MSB first):
 *   [0] = message type   1 HELLO, 2 KEEPALIVE, 6 ACTION, 7 BYE, 8 END, 9 ?
 *   [1..4] = payload (slot / action nibbles; 0 when unused)
 *   [5] = sender cube id (varies per cube per session)
 *   [6] = trailer, always 0x9 in every observed packet (559/559)
 */
#pragma once
#include <Arduino.h>

#define CW_BIT0_LOW_US   94   // low pulse for a 0 bit (toy measured 92-113)
#define CW_BIT1_LOW_US  174   // low pulse for a 1 bit (toy measured 174-176)
#define CW_BIT0_MAX_US  115   // decoder boundary: low <= 115  -> 0
#define CW_BIT1_MIN_US  150   // decoder boundary: low >= 150  -> 1

#define CW_PRE_LOW_US   234
#define CW_PRE_HIGH_US   53
#define CW_SYNC_LOW_US   94   // third preamble low

#define CW_GAP_INTRA_US 100   // high gap between bits of one nibble
#define CW_GAP_INTER_US 200   // high gap between nibbles
#define CW_GAP_FIRST_US 142   // high gap before the first bit (measured 136-142)
#define CW_END_HIGH_US   57   // postamble high (measured 56-57)
#define CW_POST_LOW_US   93   // postamble low (measured 92-94)

// Message types (nibble 0)
#define CW_TYPE_HELLO      0x1
#define CW_TYPE_KEEPALIVE  0x2
#define CW_TYPE_ACTION     0x6
#define CW_TYPE_BYE        0x7
#define CW_TYPE_END        0x8
#define CW_TYPE_UNKNOWN9   0x9

#define CW_TRAILER 0x9

// Bit-banged driver. `pin` is the single data line (3.3V logic).
class CubeProtocol {
 public:
  explicit CubeProtocol(uint8_t pin) : pin_(pin) {}

  // --- transmit -----------------------------------------------------------
  // Full 7-nibble packet. Blocks for ~7.4 ms.
  void sendPacket(const uint8_t packet[7]);
  // Attention pulse only (what the original .ino used to poll for peers).
  void sendAttention();

  // --- receive ------------------------------------------------------------
  // Blocks until a full packet is seen or `timeout_us` elapses.
  // Returns true and fills packet[7] on success.
  bool readPacket(uint8_t packet[7], uint32_t timeout_us = 25000);
  // Non-blocking attention probe: true if another cube is answering.
  bool pollForPeer(uint32_t wait_us = 2000);

  // --- message helpers ------------------------------------------------------
  static uint8_t type(const uint8_t p[7])        { return p[0]; }
  static uint8_t slot(const uint8_t p[7])        { return p[1]; }
  static uint8_t id(const uint8_t p[7])          { return p[5]; }
  static bool trailerOk(const uint8_t p[7])      { return p[6] == CW_TRAILER; }
  static const char *typeName(uint8_t t);

  static void mkHello(uint8_t id, uint8_t p[7]);
  static void mkKeepalive(uint8_t slot, uint8_t id, uint8_t p[7]);
  static void mkAction(uint8_t a1, uint8_t a2, uint8_t a3, uint8_t id, uint8_t p[7]);
  static void mkBye(uint8_t id, uint8_t p[7]);
  static void mkEnd(uint8_t id, uint8_t p[7]);

 private:
  uint8_t pin_;
  void bitLow(uint32_t us);
  void bitHigh(uint32_t us);
  // measure one pulse of `want_level` starting now; returns duration us, 0 on timeout
  uint32_t measurePulse(bool wantLevel, uint32_t timeout_us);
};
