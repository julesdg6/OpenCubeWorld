// sim_arduino.h - single-wire bus simulator shared between the protocol
// codec under test and the test driver.
#pragma once
#include <cstdint>

struct WirePulse { int level; int us; };

struct SimState {
  int drive;          // -1 = input (follow remote), 0/1 = driven level
  long t;             // virtual time (us)
  const WirePulse *remote;
  int remoteN;
  int remoteIdx;
  long edgeAt;
  int remoteLevel;
  int traceLevel;
  long traceStart;
  long trace[512];    // flat level,duration pairs
  int traceN;
  long calls;
  bool overflow;
};

extern SimState sim;

// test-driver helpers
void simReset(const WirePulse *remote, int remoteN);
void simFinishTrace();                 // close open trace segment
long simTraceTotal();                  // sum of trace durations (us)
int simTraceSegs();                    // number of (level,duration) segments
int simTraceLevel(int i);
long simTraceDur(int i);
