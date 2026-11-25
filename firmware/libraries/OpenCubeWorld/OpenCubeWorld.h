/*
 * OpenCubeWorld.h - umbrella header for the OpenCubeWorld library.
 *
 * Authentic 32x32 1-bit monochrome Radica Cube World on the
 * Waveshare ESP32-S3-LCD-1.54 (and ESP32-S3-Touch-LCD-1.54):
 *   - Lcd32 / LcdCompositor : 32x32 monochrome STN LCD renderer (240x240 RGB565 FB)
 *   - Qmi8658               : 6-axis IMU driver (tilt, flip tumble, shake dizzy)
 *   - CubeProtocol          : Radica Cube World single-wire protocol codec
 *   - World                 : autonomous stickman + linked-cube world engine
 *   - cst816d               : optional capacitive touch driver (auto-detected)
 */
#pragma once
#include "config.h"
#include "display.h"
#include "cubeprotocol.h"
#include "cst816d.h"
#include "qmi8658.h"
#include "lcd32.h"
#include "world.h"
