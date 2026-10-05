#pragma once

#include <Arduino.h>

#include "PofoSmartCablePhy.h"

class PofoSmartCablePhyESP32 : public PofoSmartCablePhy {
 public:
  PofoSmartCablePhyESP32();

 protected:
  bool configurePins(int clkIn, int dataIn, int clkOut, int dataOut);
  uint32_t nowMicros() const;
  bool readClock() const;
  bool readData() const;
  void writeClock(bool high);
  void writeData(bool high);
  void platformDelayMicros(uint32_t microseconds);
  void idle();

 private:
  static void IRAM_ATTR onClkInChange(void* argument);

  int clkIn_;
  int dataIn_;
  int clkOut_;
  int dataOut_;
};
