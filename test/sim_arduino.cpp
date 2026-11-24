// sim_arduino.cpp - Arduino API + single-wire bus simulation.
//
// The codec (cubeprotocol.cpp) is compiled against stubinc/Arduino.h which
// only declares the API. This file provides the implementation: a virtual
// clock plus a remote endpoint that can replay a recorded pulse train.
//
//   - While the local pin is OUTPUT, we "drive" the wire: the driven level
//     is recorded into the trace (level,duration segments).
//   - While the local pin is INPUT, the wire follows the remote pulse
//     train (or idle-high once it is exhausted).
//
// delayMicroseconds() advances virtual time. When input, it steps in
// remote-edge increments to model a fast polling loop without a busy
// infinite loop.
#include "sim_arduino.h"
#include <Arduino.h>
#include <cstring>
#include <cstdio>

SimState sim;

static void traceSet(int level) {
  if (sim.traceLevel == level) return;
  if (sim.traceLevel >= 0 && sim.traceN + 2 <= 512) {
    long dur = sim.t - sim.traceStart;
    if (dur > 0) {
      sim.trace[sim.traceN++] = sim.traceLevel;
      sim.trace[sim.traceN++] = dur;
    }
  }
  sim.traceLevel = level;
  sim.traceStart = sim.t;
}

static void advanceToNextEdge() {
  while (sim.remoteIdx < sim.remoteN && sim.edgeAt <= sim.t) {
    sim.remoteIdx++;
    if (sim.remoteIdx < sim.remoteN) {
      sim.remoteLevel = sim.remote[sim.remoteIdx].level;
      sim.edgeAt = sim.t + sim.remote[sim.remoteIdx].us;
    }
  }
}

static int remoteLevelAt() {
  if (sim.remoteIdx < sim.remoteN) return sim.remoteLevel;
  return 1;  // idle high
}

unsigned long micros() { return (unsigned long)sim.t; }
unsigned long millis() { return (unsigned long)(sim.t / 1000); }
void delay(unsigned long) {}

void delayMicroseconds(unsigned int us) {
  long target = sim.t + us;
  if (sim.drive >= 0) {
    sim.t = target;
  } else {
    while (sim.t < target) {
      if (sim.remoteIdx < sim.remoteN) {
        long nxt = sim.edgeAt;
        if (nxt > target) nxt = target;
        sim.t = nxt;
      } else {
        sim.t = target;
      }
      advanceToNextEdge();
    }
  }
}

void pinMode(uint8_t, uint8_t mode) {
  if (mode == INPUT) {
    sim.drive = -1;
    // with no remote, a floating pin sits idle-high (pull-up terminated)
    traceSet(sim.remote ? -1 : 1);
  } else {
    if (sim.drive < 0) sim.drive = 1;
  }
}

void digitalWrite(uint8_t, int lvl) {
  if (sim.drive >= 0) {
    sim.drive = lvl;
    traceSet(lvl);
  }
}

int digitalRead(uint8_t) {
  sim.calls++;
  if (sim.calls > 50000000) { sim.overflow = true; return sim.drive >= 0 ? sim.drive : 1; }
  if (sim.drive >= 0) return sim.drive;
  // model a fast polling loop: each read costs ~1us of virtual time;
  // if the next remote edge falls inside that step, land exactly on it.
  if (sim.remoteIdx < sim.remoteN) {
    long toEdge = sim.edgeAt - sim.t;
    if (toEdge < 0) toEdge = 0;
    sim.t += (toEdge > 1) ? 1 : toEdge;
  } else {
    sim.t += 1;
  }
  advanceToNextEdge();
  return remoteLevelAt();
}

// ---- test-driver helpers -------------------------------------------------
void simReset(const WirePulse *remote, int remoteN) {
  std::memset(&sim, 0, sizeof sim);
  sim.drive = -1;
  sim.traceLevel = -1;
  sim.remote = remote;
  sim.remoteN = remoteN;
  if (remoteN > 0) {
    sim.remoteLevel = remote[0].level;
    sim.edgeAt = remote[0].us;
  }
}

void simFinishTrace() { traceSet(-1); }

long simTraceTotal() {
  long total = 0;
  for (int i = 0; i + 1 < sim.traceN; i += 2) total += sim.trace[i + 1];
  return total;
}

int simTraceSegs() { return sim.traceN / 2; }
int simTraceLevel(int i) { return sim.trace[i * 2]; }
long simTraceDur(int i) { return sim.trace[i * 2 + 1]; }
