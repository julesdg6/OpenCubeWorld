// protocol_test.cpp - host-side functional test for cubeprotocol.cpp.
//
// Simulates the single-wire bus (see sim_arduino.{h,cpp}):
//  - TRANSMIT: record the pulse train the codec drives and compare it
//    against the framing measured on real captures.
//  - RECEIVE: feed the REAL pulse trains extracted from
//    1Whip_1Dodger.sr (2 MHz logic analyzer samples) into readPacket().
//
// Build:
//   g++ -std=gnu++17 -Wall -I. -Istubinc
//       protocol_test.cpp sim_arduino.cpp cubeprotocol.cpp -o protocol_test
#include <cstdio>
#include "sim_arduino.h"
#include "test_vectors.h"
#include "cubeprotocol.h"

static int failures = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } \
  } while (0)

static void testTransmit() {
  simReset(nullptr, 0);
  pinMode(4, INPUT);
  // the packet the original Arduino .ino sent (first line of the capture)
  uint8_t p[7] = {0x1, 0x7, 0x1, 0x5, 0x1, 0x1, 0x9};
  CubeProtocol proto(4);
  proto.sendPacket(p);
  simFinishTrace();

  // expected framing (measured constants from the capture)
  // L234 H53 L94 | 28x[Hgap Lbit] | H57 L93
  int exp[61][2];
  int n = 0;
  exp[n][0] = 0; exp[n][1] = 234; n++;
  exp[n][0] = 1; exp[n][1] = 53;  n++;
  exp[n][0] = 0; exp[n][1] = 94;  n++;
  for (int bit = 0; bit < 28; bit++) {
    uint8_t nib = p[bit / 4];
    int one = (nib & (0x08 >> (bit & 3))) != 0;
    int gap = (bit == 0) ? 142 : (((bit & 3) == 0 && bit < 24) ? 200 : 100);
    exp[n][0] = 1; exp[n][1] = gap;    n++;
    exp[n][0] = 0; exp[n][1] = one ? 174 : 94; n++;
  }
  exp[n][0] = 1; exp[n][1] = 57; n++;
  exp[n][0] = 0; exp[n][1] = 93; n++;

  int segs = simTraceSegs();
  CHECK(segs == n, "tx trace size %d != expected %d", segs, n);
  for (int i = 0; i < segs && i < n; i++) {
    CHECK(simTraceLevel(i) == exp[i][0] && simTraceDur(i) == exp[i][1],
          "tx[%d] = %d x %ld us, expected %d x %d us",
          i, simTraceLevel(i), simTraceDur(i), exp[i][0], exp[i][1]);
  }
  long total = simTraceTotal();
  CHECK(total >= 6900 && total <= 7800, "tx total %ld us not in 6.9-7.8 ms (captures: 7.0-7.6)", total);
}

static void testReceive() {
  for (int k = 0; k < 4; k++) {
    simReset(test_pulses[k], test_pulse_counts[k]);
    pinMode(4, INPUT);
    uint8_t p[7] = {0};
    CubeProtocol proto(4);
    bool ok = proto.readPacket(p, 20000);
    CHECK(ok, "rx[%d]: readPacket failed", k);
    if (ok) {
      for (int i = 0; i < 7; i++) {
        CHECK(p[i] == test_expected[k][i],
              "rx[%d] nibble %d = %d, expected %d", k, i, p[i], test_expected[k][i]);
      }
      CHECK(CubeProtocol::trailerOk(p), "rx[%d]: trailer != 9", k);
    }
    CHECK(!sim.overflow, "rx[%d]: digitalRead call limit hit (loop?)", k);
  }
}

static void testPollForPeer() {
  // peer answers with a full keepalive packet after the attention
  simReset(test_pulses[2], test_pulse_counts[2]);
  pinMode(4, INPUT);
  CubeProtocol proto(4);
  bool peer = proto.pollForPeer(20000);
  CHECK(peer, "pollForPeer should see the remote answer");
}

int main() {
  testTransmit();
  testReceive();
  testPollForPeer();
  if (failures == 0) printf("ALL PROTOCOL TESTS PASSED\n");
  else printf("%d FAILURES\n", failures);
  return failures ? 1 : 0;
}
