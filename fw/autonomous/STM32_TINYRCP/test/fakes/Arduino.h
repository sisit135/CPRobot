// SPDX-License-Identifier: GPL-3.0-only

// Test declarations only; not a STM32 SDK.
#pragma once
#include <cstdint>
constexpr int LOW = 0, HIGH = 1, OUTPUT = 1, INPUT = 0;
constexpr int INPUT_PULLUP = 2, OUTPUT_OPEN_DRAIN = 3;
constexpr int PA1 = 1, PA2 = 2, PA3 = 3, PA4 = 4, PB0 = 16, PB1 = 17, PB3 = 19, PA8 = 8, PA9 = 9, PB8 = 24, PB9 = 25,
              PA6 = 6, PA7 = 7, PB6 = 22, PB7 = 23, PB10 = 26, PB12 = 28, PB13 = 29;
unsigned long micros();
void pinMode(int, int);
void digitalWrite(int, int);
void analogWrite(int, int);
void analogWriteResolution(int);
int digitalRead(int);
void delayMicroseconds(unsigned int);

void delay(unsigned long);
