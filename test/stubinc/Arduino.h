// Minimal Arduino.h for host-side protocol tests: types, macros and
// declarations only. The wire/timing simulation lives in sim_arduino.cpp.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>

#define HIGH 1
#define LOW 0
#define INPUT 0x01
#define OUTPUT 0x02
#define INPUT_PULLUP 0x03
#define PROGMEM
#define F(x) x

unsigned long micros();
unsigned long millis();
void delayMicroseconds(unsigned int us);
void delay(unsigned long ms);
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, int value);
int digitalRead(uint8_t pin);
