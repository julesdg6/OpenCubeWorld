# Host-side protocol tests

Verifies `cubeprotocol.{h,cpp}` against the **raw logic-analyzer
samples** from the captures (no Arduino needed):

- **Receive:** replays the real recorded pulse trains of the first four
  packets of `1Whip_1Dodger.sr` (2 MHz samples) through
  `readPacket()` and checks all 28 bits + trailer.
- **Transmit:** checks that `sendPacket()` drives the exact pulse
  train the real toys drive (61 segments, 7.4 ms total).
- **Peer probe:** `pollForPeer()` against a recorded answer.

`test_vectors.h` is generated from the `.sr` capture
(`protocol-captures/1Whip_1Dodger/1Whip_1Dodger.sr` in
[npjg/radica-cube-world-hack](https://github.com/npjg/radica-cube-world-hack));
it is checked in so the test builds standalone.

Build & run (g++, C++17):

    g++ -std=gnu++17 -Wall -I. -Istubinc \
        -I../firmware/libraries/OpenCubeWorld \
        protocol_test.cpp sim_arduino.cpp \
        ../firmware/libraries/OpenCubeWorld/cubeprotocol.cpp \
        -o protocol_test
    ./protocol_test

Expected output: `ALL PROTOCOL TESTS PASSED`.
