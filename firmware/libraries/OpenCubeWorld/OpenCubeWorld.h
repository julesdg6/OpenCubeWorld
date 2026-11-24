/*
 * OpenCubeWorld.h - umbrella header for the OpenCubeWorld Arduino library.
 *
 * Waveshare 1.54inch Touch LCD Module (SKU 34370): ST7789S 240x240 SPI
 * display + CST816D I2C capacitive touch.
 *
 * Components:
 *   - CubeProtocol : Radica Cube World single-wire protocol codec
 *   - World        : display engine (cubes, animation, protocol monitor)
 *   - cst816d      : touch driver (vendored from Waveshare)
 */
#pragma once
#include "config.h"
#include "display.h"
#include "cubeprotocol.h"
#include "cst816d.h"
#include "world.h"
