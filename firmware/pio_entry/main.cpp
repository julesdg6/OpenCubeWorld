/*
 * PlatformIO entry wrapper selecting which Arduino sketch to compile.
 * Keeps all firmware/*.ino sketches 100% compatible with Arduino IDE.
 */
#include <Arduino.h>
#include <OpenCubeWorld.h>

#if defined(OCW_SKETCH_CUBELINK)
#include "../CubeLink/CubeLink.ino"
#elif defined(OCW_SKETCH_REPLAY_DODGER)
#include "../Replay_1Whip_1Dodger/Replay_1Whip_1Dodger.ino"
#else
#include "../OpenCubeWorld/OpenCubeWorld.ino"
#endif
