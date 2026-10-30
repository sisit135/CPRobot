#pragma once
#include <cstdint>
constexpr int MSBFIRST = 1, SPI_MODE0 = 0;
struct SPISettings {
  SPISettings(unsigned long, int, int) {}
};
struct FakeSPI {
  void begin() {}
  void beginTransaction(const SPISettings &) {}
  void endTransaction() {}
  uint8_t transfer(uint8_t);
};
extern FakeSPI SPI;
