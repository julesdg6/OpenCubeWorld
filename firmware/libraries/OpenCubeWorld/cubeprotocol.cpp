/*
 * cubeprotocol.cpp - Radica Cube World protocol codec, bit-banged.
 * Timing verified against raw sigrok captures; see docs/PROTOCOL.md.
 */
#include "cubeprotocol.h"

void CubeProtocol::bitLow(uint32_t us) {
  pinMode(pin_, OUTPUT);
  digitalWrite(pin_, LOW);
  delayMicroseconds(us);
  pinMode(pin_, INPUT);           // float high (bus is pull-up terminated)
}

void CubeProtocol::bitHigh(uint32_t us) {
  pinMode(pin_, INPUT);
  delayMicroseconds(us);
}

uint32_t CubeProtocol::measurePulse(bool wantLevel, uint32_t timeout_us) {
  uint32_t t0 = micros();
  while (digitalRead(pin_) != (wantLevel ? HIGH : LOW)) {
    if (micros() - t0 > timeout_us) return 0;
  }
  t0 = micros();
  while (digitalRead(pin_) == (wantLevel ? HIGH : LOW)) {
    if (micros() - t0 > timeout_us) return 0;
  }
  return micros() - t0;
}

void CubeProtocol::sendAttention() {
  bitLow(CW_PRE_LOW_US);
  bitHigh(CW_PRE_HIGH_US);
  bitLow(CW_SYNC_LOW_US);
}

void CubeProtocol::sendPacket(const uint8_t packet[7]) {
  // Preamble
  bitLow(CW_PRE_LOW_US);
  bitHigh(CW_PRE_HIGH_US);
  bitLow(CW_SYNC_LOW_US);

  // 28 bits, MSB first. The high gap precedes each bit's low pulse.
  for (int bit = 0; bit < 28; bit++) {
    uint8_t nibble = packet[bit / 4];
    bool one = (nibble & (0x08 >> (bit & 3))) != 0;
    // Toy quirk: ~140us gap before bit 0, ~100us before bit 24.
    uint32_t gap;
    if (bit == 0) gap = CW_GAP_FIRST_US;
    else if ((bit & 3) == 0 && bit < 24) gap = CW_GAP_INTER_US;
    else gap = CW_GAP_INTRA_US;
    bitHigh(gap);
    bitLow(one ? CW_BIT1_LOW_US : CW_BIT0_LOW_US);
  }

  // Postamble
  bitHigh(CW_END_HIGH_US);
  bitLow(CW_POST_LOW_US);
}

bool CubeProtocol::readPacket(uint8_t packet[7], uint32_t timeout_us) {
  // Preamble: L(200-300) H(40-80) L(60-130)
  uint32_t t0 = micros();
  uint32_t d;
  while (micros() - t0 < timeout_us) {
    d = measurePulse(false, 4000);
    if (d == 0 || d < 200 || d > 300) continue;
    d = measurePulse(true, 200);
    if (d == 0 || d < 40 || d > 80) continue;
    d = measurePulse(false, 200);
    if (d == 0 || d < 60 || d > 130) continue;
    break;
  }
  if (d == 0) return false;

  uint32_t value = 0;
  for (int bit = 0; bit < 28; bit++) {
    d = measurePulse(true, 400);
    if (d == 0 || d <= 80) return false;          // end-of-bits gap = premature end
    uint32_t low = measurePulse(false, 400);
    if (low == 0) return false;
    if (low <= CW_BIT0_MAX_US) {
      value <<= 1;
    } else if (low >= CW_BIT1_MIN_US) {
      value = (value << 1) | 1;
    } else {
      return false;                               // bad bit length
    }
  }

  // Postamble: H(<=80) L(60-130)
  measurePulse(true, 200);
  d = measurePulse(false, 200);
  if (d == 0 || d < 60 || d > 130) return false;

  for (int i = 0; i < 7; i++) {
    packet[i] = (value >> ((6 - i) * 4)) & 0x0F;
  }
  return true;
}

bool CubeProtocol::pollForPeer(uint32_t wait_us) {
  sendAttention();
  return measurePulse(true, wait_us) > 0;
}

const char *CubeProtocol::typeName(uint8_t t) {
  switch (t) {
    case CW_TYPE_HELLO:     return "HELLO";
    case CW_TYPE_KEEPALIVE: return "KEEP";
    case CW_TYPE_ACTION:    return "WHIP";
    case CW_TYPE_BYE:       return "BYE";
    case CW_TYPE_END:       return "END";
    case CW_TYPE_UNKNOWN9:  return "UNK9";
    default:                return "?";
  }
}

void CubeProtocol::mkHello(uint8_t id, uint8_t p[7]) {
  // Observed: [1, 7|f, 1, 5, 1|2, id, 9]
  p[0] = CW_TYPE_HELLO; p[1] = 0xF; p[2] = 1; p[3] = 5; p[4] = 1; p[5] = id; p[6] = CW_TRAILER;
}
void CubeProtocol::mkKeepalive(uint8_t slot, uint8_t id, uint8_t p[7]) {
  // Observed: [2, slot, 0, 0, 0, id, 9]
  p[0] = CW_TYPE_KEEPALIVE; p[1] = slot; p[2] = 0; p[3] = 0; p[4] = 0; p[5] = id; p[6] = CW_TRAILER;
}
void CubeProtocol::mkAction(uint8_t a1, uint8_t a2, uint8_t a3, uint8_t id, uint8_t p[7]) {
  // Observed: [6, a1, a2, a3, 0, id, 9]  with a1 in {1,2}, a2 in {0,2}
  p[0] = CW_TYPE_ACTION; p[1] = a1; p[2] = a2; p[3] = a3; p[4] = 0; p[5] = id; p[6] = CW_TRAILER;
}
void CubeProtocol::mkBye(uint8_t id, uint8_t p[7]) {
  // Observed: [7, 0, 0, 0, 0, id, 9]
  p[0] = CW_TYPE_BYE; p[1] = 0; p[2] = 0; p[3] = 0; p[4] = 0; p[5] = id; p[6] = CW_TRAILER;
}
void CubeProtocol::mkEnd(uint8_t id, uint8_t p[7]) {
  // Observed: [8, 0, 0, 0, 0, id, 9]
  p[0] = CW_TYPE_END; p[1] = 0; p[2] = 0; p[3] = 0; p[4] = 0; p[5] = id; p[6] = CW_TRAILER;
}
